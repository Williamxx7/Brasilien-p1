import React from 'react';
import {StyleSheet, Button, View, Text, Alert} from 'react-native';

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
      <Button
        onPress={() => Alert.alert('Pressed!')}
        title="Connect to Weight"
        color="#0047ba"
      />
    </View>
  );
}
