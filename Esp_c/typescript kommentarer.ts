// bleConfig.ts

// Service + Characteristics UUIDs
export const SERVICE_UUID = 'ab49b033-1163-48db-931c-9c2a3002ee1d'; 

export const USER_INFO_CHAR_UUID   = 'ab49b033-1163-48db-931c-9c2a3002ee1f'; // UUID for characteristic hvor vi sender brugerinfo ned til ESP32
export const COMMAND_CHAR_UUID     = 'ab49b033-1163-48db-931c-9c2a3002ee1e'; // UUID for characteristic hvor vi sender kommandoer til ESP32
export const WEIGHT_DATA_CHAR_UUID = 'ab49b033-1163-48db-931c-9c2a3002ee20'; // UUID for characteristic hvor vægten sender vejedata tilbage til appen

// Evt. type hvis du vil være stram med 1|2|3
export type WeightNumber = 1 | 2 | 3;

// Skift disse værdier til dine rigtige vægte (navn eller id fra scan)
export const WEIGHT_TARGETS: Record<WeightNumber, string> = {
  1: 'ESP32_Weigh_1', // overvej at erstatte med ID
  2: 'ESP32_Weigh_2',
  3: 'ESP32_Weigh_3',
};

// useScaleBle.ts
import { useEffect, useRef, useState } from 'react';
import { Alert } from 'react-native';
import { BleManager, Device } from 'react-native-ble-plx';
import base64 from 'react-native-base64';

import {
  SERVICE_UUID,
  COMMAND_CHAR_UUID,
  WEIGHT_TARGETS,
  WeightNumber,
  USER_INFO_CHAR_UUID,
  WEIGHT_DATA_CHAR_UUID,
} from './bleConfig';

type UseScaleBleReturn = { // Interface der beskriver hvad hooken returnerer til resten af appen
  scanning: boolean;   // Om appen lige nu scanner efter vægte via BLE
  connectedDeviceId: string | null; // Id for det device vi er forbundet til, ellers null hvis ingen forbindelse
  selectedWeight: WeightNumber | null;  // Hvilken vægt (1, 2, 3) brugeren har valgt
  currentWeight: number | null;  // Aktuel målt vægt i kg, ellers null hvis ingen måling endnu
  isWeightStable: boolean;  // Om vægten vurderes som stabil (ikke hopper for meget op og ned)
  startScanForWeight: (weightNum: WeightNumber, userId: string, material: string) => void;  // Starter scanning efter en bestemt vægt og sender brugerinfo til vægten
  sendStartCommand: () => void; // Sender START kommando til vægten og begynder at lytte på vægtdata
  sendConfirmResult: () => Promise<void>;  // Sender CONFIRM_RESULT kommando til vægten når bruger bekræfter mælingen
  resetAndMeasureAgain: () => void;  // Nulstiller måling og starter en ny mæling
  stopScan: () => void;  // Stopper BLE scanning manuelt
};

// Grænse for hvor meget vægten må svinge for at vi siger den er stabil
const STABILITY_THRESHOLD = 0.005; // kg (5 grams)
const STABILITY_READINGS_COUNT = 5; // Hvor mange seneste målinger vi bruger til at vurdere stabilitet

export function useScaleBle(): UseScaleBleReturn {
  // React state til at holde styr på scanning og måledata
  const [scanning, setScanning] = useState(false);
  const [connectedDeviceId, setConnectedDeviceId] = useState<string | null>(null);
  const [selectedWeight, setSelectedWeight] = useState<WeightNumber | null>(null);
  const [currentWeight, setCurrentWeight] = useState<number | null>(null);
  const [isWeightStable, setIsWeightStable] = useState(false);

  // Ref til vores BleManager instance sa vi kun opretter den en gang
  const managerRef = useRef<BleManager | null>(null);
  // Ref der gemmer historik over seneste vægtmål til stabilitetscheck
  const weightReadingsRef = useRef<number[]>([]);

  // Funktion der checker om vægten er stabil baseret på en liste af målinger
  const checkWeightStability = (readings: number[]): boolean => {
    // Hvis vi har for få målinger kan vi ikke vurdere stabilitet
    if (readings.length < STABILITY_READINGS_COUNT) {
      return false;
    }

    // Tag de seneste N målinger
    const recentReadings = readings.slice(-STABILITY_READINGS_COUNT);
    const min = Math.min(...recentReadings);
    const max = Math.max(...recentReadings);

    // Hvis forskellen mellem min og max er mindre eller lig med threshold, er vægten stabil
    return (max - min) <= STABILITY_THRESHOLD;
  };

  // useEffect kører en gang når hooken bliver brugt første gang
  // Her opretter vi BLE manageren og rydder op når komponenten unmountes
  useEffect(() => {
    // Opretter en ny BleManager og gemmer den i vores ref
    const manager = new BleManager();
    managerRef.current = manager;

    // Cleanup funktion når komponenten bliver fjernet
    return () => {
      if (managerRef.current) {
        // Stop igangværende scanning hvis der er en
        managerRef.current.stopDeviceScan();
        // Frigiv BLE ressourcer
        managerRef.current.destroy();
        // Nulstil referencen
        managerRef.current = null;
      }
    };
  }, []);

  // Funktion der starter scanning efter en bestemt vægt
  // weightNum er vægtnummer, userId og material sendes ned til ESP32
  const startScanForWeight = (weightNum: WeightNumber, userId: string, material: string) => {
    const manager = managerRef.current;
    if (!manager) {
      // Hvis BLE manageren ikke er klar, viser vi fejl
      Alert.alert('Bluetooth', 'BLE manager er ikke initialiseret.');
      return;
    }

    // Gemmer hvilken vægt der er valgt til visning i UI
    setSelectedWeight(weightNum);

    // Hent target navn eller id for den vægt vi vil finde
    const target = WEIGHT_TARGETS[weightNum];
    if (!target) {
      Alert.alert(
        'Bluetooth',
        `Ingen target sat for vægt ${weightNum}.\nOpdater WEIGHT_TARGETS i bleConfig.ts til dit rigtige navn eller id.`,
      );
      return;
    }

    // Hvis vi allerede scanner skal vi ikke starte en ny scanning
    if (scanning) {
      console.log('Already scanning, ignoring new request');
      return;
    }

    console.log('Starting BLE PLX scan for weight', weightNum, 'target', target);
    setScanning(true);

    // Variable der holder den enhed vi finder, hvis vi finder en
    let found: Device | null = null;

    // Start scanning efter BLE enheder uden filter
    manager.startDeviceScan(null, null, (error, device) => {
      if (error) {
        // Fejl håndtering hvis scannet fejler
        console.error('Scan error', error);
        setScanning(false);
        manager.stopDeviceScan();
        Alert.alert('Fejl', error.message ?? 'Ukendt fejl ved scanning.');
        return;
      }

      // Hvis der ikke er noget device i callback gør vi ikke noget
      if (!device) {
        return;
      }

      // Hent navn og id fra enheden
      const name = device.name ?? device.localName ?? '';
      const id = device.id;

      // Match enten på navn eller på id imod vores target
      if (name === target || id === target) {
        console.log('Found target device:', { name, id });
        found = device;

        // Vi har fundet vægten sa vi stopper scanning
        manager.stopDeviceScan();
        setScanning(false);

        Alert.alert('Bluetooth', `Fandt vægt ${weightNum}. Forbinder...`);

        // Forsoger at forbinde til den fundne enhed
        manager
          .connectToDevice(id)
          .then(connected => {
            console.log('Connected to device', connected.id);
            // Gemmer id for den tilsluttede enhed sa vi kan bruge det senere
            setConnectedDeviceId(connected.id);
            Alert.alert('Bluetooth', `Forbundet til vægt ${weightNum}`);

            // Vi skal opdage alle services og characteristics før vi kan læse eller skrive
            return connected.discoverAllServicesAndCharacteristics();
          })
          .then(connected => {
            console.log('Services og characteristics opdaget');

            // Bygger en streng med brugerinfo fx USER:1234;MAT:PLASTIC
            const userInfo = `USER:${userId};MAT:${material}`;
            console.log('Sender USER INFO payload:', userInfo);

            // Skriv brugerinfo til USER INFO characteristic base64 kodet
            return connected.writeCharacteristicWithResponseForService(
              SERVICE_UUID,
              USER_INFO_CHAR_UUID,
              base64.encode(userInfo),
            );
          })
          .then(characteristic => {
            // Log at vi har skrevet korrekt til characteristicen
            console.log('Wrote to characteristic', characteristic.uuid);
          })
          .catch(err => {
            // Fejl ved forbindelse eller skrivning
            console.error('Connect eller write error', err);
            Alert.alert('Fejl', 'Kunne ikke forbinde eller skrive til vægten.');
          });
      }
    });

    // Fail safe timeout der stopper scanning efter 8 sekunder hvis ingen enhed er fundet
    setTimeout(() => {
      if (!managerRef.current) {
        return;
      }
      if (!found && scanning) {
        console.log('Scan timeout, no device found for weight', weightNum);
        managerRef.current.stopDeviceScan();
        setScanning(false);
        Alert.alert('Bluetooth', `Fandt ikke vægt ${weightNum}.`);
      }
    }, 8000);
  };

  // Funktion til manuelt at stoppe scanning
  const stopScan = () => {
    const manager = managerRef.current;
    if (!manager) {
      return;
    }

    console.log('Stopping BLE scan manually');
    manager.stopDeviceScan();
    setScanning(false);
  };

  // Funktion der sender START kommandoen og abonnerer på vejedata
  const sendStartCommand = async () => {
    if (!connectedDeviceId) {
      // Hvis vi ikke har en aktiv forbindelse kan vi ikke sende kommando
      Alert.alert("Fejl", "Ingen enhed forbundet");
      return;
    }

    const manager = managerRef.current;
    if (!manager) return;

    try {
      console.log("Sending START command...");

      // Sikrer at vi er forbundet til enheden og har services klar
      const device = await manager.connectToDevice(connectedDeviceId);
      await device.discoverAllServicesAndCharacteristics();

      // Skriv streng "START" til COMMAND characteristic base64 kodet
      await device.writeCharacteristicWithResponseForService(
        SERVICE_UUID,
        COMMAND_CHAR_UUID,
        base64.encode("START")
      );

      console.log("START command sent");

      // Nulstil tidligere målinger og stabilitetsflag
      weightReadingsRef.current = [];
      setIsWeightStable(false);

      // Abonner på notifikationer fra WEIGHT DATA characteristic
      device.monitorCharacteristicForService(
        SERVICE_UUID,
        WEIGHT_DATA_CHAR_UUID,
        (error, characteristic) => {
          if (error) {
            console.error("Weight notification error:", error);
            return;
          }
          if (characteristic?.value) {
            // Dekod base64 streng til alm tekst
            const decoded = base64.decode(characteristic.value);
            // Parse teksten til tal
            const weight = parseFloat(decoded);
            console.log("Received weight:", weight);
            // Opdater aktuel vægt i state
            setCurrentWeight(weight);

            // Gem vægten i historik til stabilitets check
            weightReadingsRef.current.push(weight);
            // Behold kun de seneste 10 målinger sa listen ikke vokser uendeligt
            if (weightReadingsRef.current.length > 10) {
              weightReadingsRef.current = weightReadingsRef.current.slice(-10);
            }

            // Udregn om vægten er stabil og opdater state
            const stable = checkWeightStability(weightReadingsRef.current);
            setIsWeightStable(stable);
          }
        }
      );

      console.log("Subscribed to weight notifications");
    } catch (err) {
      console.error("Error sending START:", err);
      Alert.alert("Fejl", "Kunne ikke sende START kommando.");
    }
  };

  // Funktion der sender CONFIRM_RESULT til vægten når bruger bekræfter måling
  const sendConfirmResult = async () => {
    if (!connectedDeviceId) {
      Alert.alert("Fejl", "Ingen enhed forbundet");
      return;
    }

    const manager = managerRef.current;
    if (!manager) return;

    try {
      console.log("Sending CONFIRM_RESULT command...");

      // Forbind og opdag services igen for en sikkerheds skyld
      const device = await manager.connectToDevice(connectedDeviceId);
      await device.discoverAllServicesAndCharacteristics();

      // Skriv streng "CONFIRM_RESULT" til COMMAND characteristic
      await device.writeCharacteristicWithResponseForService(
        SERVICE_UUID,
        COMMAND_CHAR_UUID,
        base64.encode("CONFIRM_RESULT")
      );

      console.log("CONFIRM_RESULT command sent");
      Alert.alert("Succes", "Måling bekræftet og gemt!");
    } catch (err) {
      console.error("Error sending CONFIRM_RESULT:", err);
      Alert.alert("Fejl", "Kunne ikke bekræfte måling.");
    }
  };

  // Funktion der nulstiller måling og starter en ny måling
  const resetAndMeasureAgain = () => {
    // Nulstil vejedata og stabilitetsstate
    setCurrentWeight(null);
    setIsWeightStable(false);
    weightReadingsRef.current = [];

    // Start en ny måling ved at sende START kommando igen
    sendStartCommand();

    console.log("Reset and started new measurement");
  };

  // Eksporterer alle værdier og funktioner til komponenter der bruger hooken
  return {
    scanning,
    connectedDeviceId,
    selectedWeight,
    currentWeight,
    isWeightStable,
    startScanForWeight,
    sendStartCommand,
    sendConfirmResult,
    resetAndMeasureAgain,
    stopScan,
  };
}
