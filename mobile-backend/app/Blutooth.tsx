import { StatusBar } from 'expo-status-bar';
import { useEffect, useState } from 'react';
import { Button, Platform, Text, View } from 'react-native';
import { useAndroidPermissions } from './useAndroidPermissions';

export default function App() {
  const [hasPermissions, setHasPermissions] = useState<boolean>(Platform.OS == 'ios');
  const [waitingPerm, grantedPerm] = useAndroidPermissions();

  useEffect(() => {
    if (!(Platform.OS == 'ios')){
      setHasPermissions(grantedPerm);
    }
  }, [grantedPerm])

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
      <Text>BLE Premissions enabled!</Text>
    )
    }
      <StatusBar style="auto" />
    </View>
  );
}