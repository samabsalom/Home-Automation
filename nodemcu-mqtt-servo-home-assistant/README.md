

This is based on https://github.com/mertenats/Open-Home-Automation to allow the board to control a servo motor


example home assistant config
// switch
//   - platform: mqtt
//     name: Blinds
//     state_topic: 'blinds/1/status'
//     command_topic: 'blinds/8/switch'
//     optimistic: false
// with 3v3 ground and d1 connected to a servo this should allow the servo to rotate through two values
//  i have used 0 and 90 and intend to use this to control some venetian blind sin my bedroom with home assistant
