import React from 'react';
import {StyleSheet, View, Text, Alert, TouchableOpacity} from 'react-native';

export default function Index() {
  const onPressLearnMore = () => {
    Alert.alert('Button pressed!');
  };
  return (
    <View
      style={{
        flex: 1,
        justifyContent: "center",
        alignItems: "center",
      }}
    >
      <Text style={{ marginBottom: 20 }}> Press the Button to connect to the weigth.</Text>
  
      <TouchableOpacity
        onPress={() => Alert.alert('Pressed!')}
        style={{
          backgroundColor: '#0047ba',
          paddingVertical: 12,
          paddingHorizontal: 32,
          borderRadius: 8,
        }}
      >
        <Text style={{ color: '#fff', fontSize: 16, fontWeight: 'bold' }}>
          Connect to Weight
        </Text>
      </TouchableOpacity>
  
    </View>
  );
}
