

# nRF52840 dongle blood pressure peripheral

## LED Blink Status

* LED1: Green LED
* LED2: Red + Blue + Green LED (RGB LED)
  * White color is created by turning on Red, Blue and Green LEDs at the same time.

Button0 is used to control the device.

* Quick press sw1(button0) to switch between advertising on/off.
* Long press sw1(button0) for 5 seconds to reset the device.
* Double press sw1(button0) to switch between is bonded or not.


|  Status                           |  LED1 Green  |  LED2 Red  |  LED2 Blue  |  LED2 Green  |
|-----------------------------------|--------------| ---------- | ----------- | ------------ |
|  Adv on, disconnected, unbouded   | Short blink  | -          | On          | -            |
|  Adv off, disconnected, unbouded  | Long blink   | -          | Off         | -            |
|  Adv on, Connected, unbouded      | Short blink  | On         | Off         | -            |
|  Adv off, Connected, unbouded     | Long blink   | On         | Off         | -            |
|  Adv on, disconnected, bonded     | Short blink  | -          | Short blink | -            |
|  Adv off, disconnected, bonded    | Long blink   | -          | Long blink  | -            |
|  Adv on, Connected, bonded        | Short blink  | On         | Off         | -            |
|  Adv off, Connected, bonded       | Long blink   | On         | Off         | -            |
|  Reset                            | On 2 sec.    | On 2 sec.  | On 2 sec.   | On 2 sec.    |


Status digital output:


```mermaid
---
title: sw1 button status
---
stateDiagram-v2
    state Adv {
        [*] --> AdvOn
        AdvOn --> AdvOff : sw1 short press
        AdvOff --> AdvOn : sw1 short press
    }
    state Connect {
        [*] --> Connected
        Connected --> Disconnected : disconnected
        Disconnected --> Connected : connected
    }
    state Bond {
        [*] --> Bonded
        Bonded --> NotBonded : sw1 double press
        NotBonded --> Bonded : sw1 double press
    }
    state Reset {
        [*] --> idle
        idle --> Resetting : sw1 long press
    }
```

```mermaid
---
title: Test status
---
stateDiagram-v2
    BootUp: Adv On Disconnected
    note left of BootUp
        LED1: Short blink
        LED2: Blue on
    end note

    AfterReset: Adv Off Disconnected
    note left of AfterReset
        LED1: Long blink
        LED2: Blue off
    end note

    Connected: Adv On Connected
    note left of Connected
        LED1: Short blink
        LED2: Red on
    end note

    ConnectedBonded: Adv On Connected Bonded
    note right of ConnectedBonded
        LED1: Short blink
        LED2: Red on
    end note

    ConnectedBondedAdvOff: Adv Off Connected Bonded
    note right of ConnectedBondedAdvOff
        LED1: Long blink
        LED2: Red on
    end note

    DisconnectedBonded: Adv On Disconnected Bonded
    note right of DisconnectedBonded
        LED1: Short blink
        LED2: Blue Short blink
    end note

    DisconnectedBondedAdvOff: Adv Off Disconnected Bonded
    note left of DisconnectedBondedAdvOff
        LED1: Long blink
        LED2: Blue Long blink
    end note


    [*] --> Ready: power on
    state Ready{
        [*] --> BootUp
        BootUp --> Connected : connected
        Connected --> DisconnectedBonded : after paired and disconnected
        DisconnectedBonded --> ConnectedBonded : connected
        DisconnectedBonded --> DisconnectedBondedAdvOff : sw1 short press
        ConnectedBonded --> DisconnectedBonded : disconnected
        ConnectedBonded --> ConnectedBondedAdvOff : sw1 short press
        ConnectedBondedAdvOff --> ConnectedBonded : sw1 short press
        ConnectedBondedAdvOff --> DisconnectedBondedAdvOff : disconnected
        DisconnectedBondedAdvOff --> DisconnectedBonded : sw1 short press
    }
    state Reset{
        
        [*] --> DoReset
        DoReset --> AfterReset: after reset
    }
    Ready --> Reset: sw1 long press (5 secodns)
    Reset --> Ready : sw1 short press
    BootUp --> AfterReset : sw1 short press
```
