// bleConfig.ts

// Service + Characteristics UUIDs
export const SERVICE_UUID = 'ab49b033-1163-48db-931c-9c2a3002ee1d';

export const USER_INFO_CHAR_UUID   = 'ab49b033-1163-48db-931c-9c2a3002ee1f';
export const COMMAND_CHAR_UUID     = 'ab49b033-1163-48db-931c-9c2a3002ee1e';
export const WEIGHT_DATA_CHAR_UUID = 'ab49b033-1163-48db-931c-9c2a3002ee20';

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

type UseScaleBleReturn = {
  scanning: boolean;
  connectedDeviceId: string | null;
  selectedWeight: WeightNumber | null;
  currentWeight: number | null;
  isWeightStable: boolean;
  startScanForWeight: (weightNum: WeightNumber, userId: string, material: string) => void;
  sendStartCommand: () => void;
  sendConfirmResult: () => Promise<void>;
  resetAndMeasureAgain: () => void;
  stopScan: () => void;
};

const STABILITY_THRESHOLD = 0.005; // kg (5 grams)
const STABILITY_READINGS_COUNT = 5;

export function useScaleBle(): UseScaleBleReturn {
  const [scanning, setScanning] = useState(false);
  const [connectedDeviceId, setConnectedDeviceId] = useState<string | null>(null);
  const [selectedWeight, setSelectedWeight] = useState<WeightNumber | null>(null);
  const [currentWeight, setCurrentWeight] = useState<number | null>(null);
  const [isWeightStable, setIsWeightStable] = useState(false);

  const managerRef = useRef<BleManager | null>(null);
  const weightReadingsRef = useRef<number[]>([]);

  const checkWeightStability = (readings: number[]): boolean => {
    if (readings.length < STABILITY_READINGS_COUNT) {
      return false;
    }

    const recentReadings = readings.slice(-STABILITY_READINGS_COUNT);
    const min = Math.min(...recentReadings);
    const max = Math.max(...recentReadings);

    return (max - min) <= STABILITY_THRESHOLD;
  };

  useEffect(() => {
    const manager = new BleManager();
    managerRef.current = manager;

    return () => {
      if (managerRef.current) {
        managerRef.current.stopDeviceScan();
        managerRef.current.destroy();
        managerRef.current = null;
      }
    };
  }, []);

  const startScanForWeight = (weightNum: WeightNumber, userId: string, material: string) => {
    const manager = managerRef.current;
    if (!manager) {
      Alert.alert('Bluetooth', 'BLE-manager er ikke initialiseret.');
      return;
    }

    setSelectedWeight(weightNum);

    const target = WEIGHT_TARGETS[weightNum];
    if (!target) {
      Alert.alert(
        'Bluetooth',
        `Ingen target sat for vægt ${weightNum}.\nOpdater WEIGHT_TARGETS i bleConfig.ts til dit rigtige navn/id.`,
      );
      return;
    }

    if (scanning) {
      console.log('Already scanning, ignoring new request');
      return;
    }

    console.log('Starting BLE-PLX scan for weight', weightNum, 'target', target);
    setScanning(true);

    let found: Device | null = null;

    manager.startDeviceScan(null, null, (error, device) => {
      if (error) {
        console.error('Scan error', error);
        setScanning(false);
        manager.stopDeviceScan();
        Alert.alert('Fejl', error.message ?? 'Ukendt fejl ved scanning.');
        return;
      }

      if (!device) {
        return;
      }

      const name = device.name ?? device.localName ?? '';
      const id = device.id;

      // Match enten på navn eller id
      if (name === target || id === target) {
        console.log('Found target device:', { name, id });
        found = device;
        manager.stopDeviceScan();
        setScanning(false);

        Alert.alert('Bluetooth', `Fandt vægt ${weightNum}. Forbinder...`);

        manager
          .connectToDevice(id)
          .then(connected => {
            console.log('Connected to device', connected.id);
            setConnectedDeviceId(connected.id);
            Alert.alert('Bluetooth', `Forbundet til vægt ${weightNum}`);

            // VIGTIGT: opdag alle services og characteristics før vi skriver
            return connected.discoverAllServicesAndCharacteristics();
          })
          .then(connected => {
            console.log('Services/characteristics discovered');

            // Byg USER-info string ud fra parametre, fx "USER:1234;MAT:ALU"
            const userInfo = `USER:${userId};MAT:${material}`;
            console.log('Sending USER_INFO payload:', userInfo);

            // Skriv Brugerens USER-INFO til ESP32 som base64
            return connected.writeCharacteristicWithResponseForService(
              SERVICE_UUID,
              USER_INFO_CHAR_UUID,
              base64.encode(userInfo),
            );
          })
          .then(characteristic => {
            console.log('Wrote to characteristic', characteristic.uuid);
          })
          .catch(err => {
            console.error('Connect/write error', err);
            Alert.alert('Fejl', 'Kunne ikke forbinde/skrive til vægten.');
          });
      }
    });

    // Stop scanning efter 8 sekunder hvis vi ikke fandt noget
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

  const stopScan = () => {
    const manager = managerRef.current;
    if (!manager) {
      return;
    }

    console.log('Stopping BLE scan manually');
    manager.stopDeviceScan();
    setScanning(false);
  };

  const sendStartCommand = async () => {
    if (!connectedDeviceId) {
      Alert.alert("Fejl", "Ingen enhed forbundet");
      return;
    }

    const manager = managerRef.current;
    if (!manager) return;

    try {
      console.log("Sending START command...");

      const device = await manager.connectToDevice(connectedDeviceId);
      await device.discoverAllServicesAndCharacteristics();

      await device.writeCharacteristicWithResponseForService(
        SERVICE_UUID,
        COMMAND_CHAR_UUID,
        base64.encode("START")
      );

      console.log("START command sent ✔️");

      // Reset weight readings when starting new measurement
      weightReadingsRef.current = [];
      setIsWeightStable(false);

      // Subscribe to weight notifications
      device.monitorCharacteristicForService(
        SERVICE_UUID,
        WEIGHT_DATA_CHAR_UUID,
        (error, characteristic) => {
          if (error) {
            console.error("Weight notification error:", error);
            return;
          }
          if (characteristic?.value) {
            const decoded = base64.decode(characteristic.value);
            const weight = parseFloat(decoded);
            console.log("Received weight:", weight);
            setCurrentWeight(weight);

            // Track readings for stability detection
            weightReadingsRef.current.push(weight);
            // Keep only the last 10 readings to avoid memory buildup
            if (weightReadingsRef.current.length > 10) {
              weightReadingsRef.current = weightReadingsRef.current.slice(-10);
            }

            // Check and update stability
            const stable = checkWeightStability(weightReadingsRef.current);
            setIsWeightStable(stable);
          }
        }
      );

      console.log("Subscribed to weight notifications ✔️");
    } catch (err) {
      console.error("Error sending START:", err);
      Alert.alert("Fejl", "Kunne ikke sende START kommando.");
    }
  };

  const sendConfirmResult = async () => {
    if (!connectedDeviceId) {
      Alert.alert("Fejl", "Ingen enhed forbundet");
      return;
    }

    const manager = managerRef.current;
    if (!manager) return;

    try {
      console.log("Sending CONFIRM_RESULT command...");

      const device = await manager.connectToDevice(connectedDeviceId);
      await device.discoverAllServicesAndCharacteristics();

      await device.writeCharacteristicWithResponseForService(
        SERVICE_UUID,
        COMMAND_CHAR_UUID,
        base64.encode("CONFIRM_RESULT")
      );

      console.log("CONFIRM_RESULT command sent ✔️");
      Alert.alert("Succes", "Måling bekræftet og gemt!");
    } catch (err) {
      console.error("Error sending CONFIRM_RESULT:", err);
      Alert.alert("Fejl", "Kunne ikke bekræfte måling.");
    }
  };

  const resetAndMeasureAgain = () => {
    // Reset weight state
    setCurrentWeight(null);
    setIsWeightStable(false);
    weightReadingsRef.current = [];

    // Send new START command
    sendStartCommand();

    console.log("Reset and started new measurement ✔️");
  };

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