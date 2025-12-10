// bleConfig.ts

// Service + Characteristics UUIDs
export const SERVICE_UUID = 'ab49b033-1163-48db-931c-9c2a3002ee1d'; // // UUID for den BLE service som skalaen eksponerer

export const USER_INFO_CHAR_UUID   = 'ab49b033-1163-48db-931c-9c2a3002ee1f'; // UUID for characteristic hvor vi sender bruger info (som fx userId, materialetype) 
export const COMMAND_CHAR_UUID     = 'ab49b033-1163-48db-931c-9c2a3002ee1e'; // UUID for characteristic hvor vi sender kommandoer til skalaen (fx START, CONFIRM)
export const WEIGHT_DATA_CHAR_UUID = 'ab49b033-1163-48db-931c-9c2a3002ee20'; // UUID for characteristic hvor vi læser vejedata tilbage fra skalaen

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

type UseScaleBleReturn = { // // Definerer hvad  returneres til React komponenten
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
... (195 linjer linjer tilbage)

