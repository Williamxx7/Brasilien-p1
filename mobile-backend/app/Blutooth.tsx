import { StatusBar } from 'expo-status-bar';
import { useEffect, useState } from 'react';
import { Button, Platform, Text, View } from 'react-native';
import { BleManager, Device } from 'react-native-ble-plx';
import { useAndroidPermissions } from './useAndroidPermissions';

const bleManager = new BleManager();

const DEVICE_NAME = "MyESP32";
const SERVICE_UUID = "ab49b033-1163-48db-931c-9c2a3002ee1d";

export default function App() {
  const [hasPermissions, setHasPermissions] = useState<boolean>(Platform.OS == 'ios');
  const [waitingPerm, grantedPerm] = useAndroidPermissions();

  const [connectionStatus, setConnectionStatus] = useState("Searching...");
  const [isConnected, setIsConnected] = useState<boolean>(false);

  useEffect(() => {
    if (!(Platform.OS == 'ios')){
      setHasPermissions(grantedPerm);
    }
  }, [grantedPerm])

  useEffect(() => {
    if(hasPermissions){
      searchAndConnectToDevice();
    }
  }, [hasPermissions]);

  const searchAndConnectToDevice = () =>
    bleManager.startDeviceScan(null, null, (error, device) => {
      if (error) {
        console.error(error);
        setIsConnected(false);
        setConnectionStatus("Error searching for devices");
        return;
      }
      if (device?.name === DEVICE_NAME) {
        bleManager.stopDeviceScan();
        setConnectionStatus("Connecting...");
        connectToDevice(device);
      }
    });


    const [device, setDevice] = useState<Device | null>(null);

    const connectToDevice = async (device: Device) => {
      try {
      const _device = await device.connect(); 
       // require to make all services and Characteristics accessable
      await _device.discoverAllServicesAndCharacteristics();
      setConnectionStatus("Connected");
      setIsConnected(true);
      setDevice(_device);
      } catch (error){
          setConnectionStatus("Error in Connection");
          setIsConnected(false);
      }
    };

    useEffect(() => {
      if (!device) {
        return;
      }

      const subscription = bleManager.onDeviceDisconnected(
        device.id,
        (error, device) => {
          if (error) {
            console.log("Disconnected with error:", error);
          }
          setConnectionStatus("Disconnected");
          setIsConnected(false);
          console.log("Disconnected device");
          if (device) {
            setConnectionStatus("Reconnecting...");
            connectToDevice(device)
              .then(() => {
                setConnectionStatus("Connected");
                setIsConnected(true);
              })
              .catch((error) => {
                console.log("Reconnection failed: ", error);
                setConnectionStatus("Reconnection failed");
                setIsConnected(false);
                setDevice(null);
              });
          }
        }
      );

      return () => subscription.remove();
    }, [device]);
  

  return (
    <View
    style={{flex: 1, alignItems: 'center', justifyContent:'center'}}
    >
    {
      !hasPermissions && (
        <View> 
          <Text>Looks like you have not enabled Permission for BLE</Text>
        </View>
      )
    }
    {hasPermissions &&(
      <View>
      <Text>BLE Premissions enabled!</Text>
      <Text>The connection status is: {connectionStatus}</Text>
      <Button 
      disabled={!isConnected} 
      onPress={() => {}}
      title={`The button is ${isConnected ? "enabled" : "disabled"}`}
      />
      </View>
    )
    }
      <StatusBar style="auto" />
    </View>
  );
}