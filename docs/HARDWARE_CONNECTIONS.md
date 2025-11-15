# Hardware Connections - ESP32-2432S028R to Arduino Nano

## ESP32-2432S028R Available Connectors

### P3 Connector
- GND
- GPIO35 (MAX6675 SO/MISO)
- GPIO22 (MAX6675 SCK)
- GPIO21 (TFT Backlight control)

### CN1 Connector
- GND
- GPIO22 (MAX6675 SCK - shared with P3)
- GPIO27 (MAX6675 CS)
- 3.3V

### P1 Connector (Serial - **USED FOR NANO COMMUNICATION**)
- **TX (GPIO1)** → Arduino Nano RX (with optocoupler)
- **RX (GPIO3)** → Arduino Nano TX (with optocoupler)
- VCC (5V output)
- GND

### Micro USB
- Programming and power (inaccessible in furnace installation)

## MAX6675 Thermocouple Connection

Connected via CN1 and P3:
- **CS**: GPIO27 (CN1)
- **SCK**: GPIO22 (CN1/P3)
- **SO**: GPIO35 (P3)

**Note**: MAX6675 and Serial communication do NOT share pins in the final design.

## Arduino Nano Relay Bridge Connection

### Signal Path (via 6N137 Optocouplers recommended)

```
ESP32 P1          6N137 Opto         Arduino Nano
---------         ------------       -------------
TX (GPIO1) -----> Input              RX (Pin 0)
                  3.3V side          5V side
                  
RX (GPIO3) <----- Output             TX (Pin 1)
                  3.3V side          5V side

GND ------------- Common GND -------- GND
```

### Optocoupler Circuit (6N137 Example)

**For TX (ESP32 → Nano):**
```
ESP32 TX (3.3V) → 220Ω → 6N137 Pin 2 (Anode)
                         6N137 Pin 3 (Cathode) → GND

5V supply → 6N137 Pin 8 (VCC)
6N137 Pin 6 (Output) → 330Ω → Nano RX
6N137 Pin 7 (Enable) → 5V
6N137 Pin 5 (GND) → GND
```

**For RX (Nano → ESP32):**
```
Nano TX (5V) → 220Ω → 6N137 Pin 2 (Anode)
                      6N137 Pin 3 (Cathode) → GND

3.3V supply → 6N137 Pin 8 (VCC)
6N137 Pin 6 (Output) → 330Ω → ESP32 RX
6N137 Pin 7 (Enable) → 3.3V
6N137 Pin 5 (GND) → GND
```

### Why Optocouplers?

1. **Voltage level translation**: 3.3V (ESP32) ↔ 5V (Nano)
2. **Electrical isolation**: Protects ESP32 from relay/SSR electrical noise
3. **Ground loop prevention**: Isolates potentially noisy relay ground from ESP32 ground

## Serial Communication Settings

- **Baud Rate**: 115200 (hardware UART, reliable at this speed)
- **Protocol**: Text-based commands (`S1,1\n`, `P2,512\n`, etc.)
- **ESP32 Side**: Hardware Serial (UART0) via `Serial` object
- **Nano Side**: Hardware Serial via `Serial` object

## Important Notes

### Production vs Development Mode

**Development Mode** (optional, `#define DEBUG_SERIAL` uncommented):
- Serial debug logs enabled via USB
- **WARNING**: Conflicts with Nano communication!
- Use ONLY for debugging; flash separate builds for production

**Production Mode** (default, `DEBUG_SERIAL` commented out):
- No serial logging (all `DEBUG_PRINT()` calls compile to nothing)
- UART0 dedicated to Nano relay communication
- USB port inaccessible in furnace installation

### Pin Conflict Resolution History

**Original Design** (problematic):
- GPIO22/GPIO35 shared between MAX6675 and SoftwareSerial
- GPIO27 used as mode-select (CS/Enable switching)
- Problem: Nano received garbage data during MAX6675 clock pulses

**Final Design** (clean):
- MAX6675: GPIO22 (SCK), GPIO27 (CS), GPIO35 (SO) - dedicated SPI
- Relay Serial: GPIO1 (TX), GPIO3 (RX) - dedicated Hardware UART
- **No pin sharing**, **no conflicts**, **no mode switching needed**

## Testing Serial Communication

### Development Testing (with USB connected):
1. Comment out `#define DEBUG_SERIAL` in `main.cpp`
2. Flash ESP32 firmware
3. Flash Nano firmware (115200 baud)
4. Connect optocoupler circuit
5. Monitor via web interface (control.html)

### Production Testing (in furnace):
1. Ensure `DEBUG_SERIAL` is commented out (production build)
2. Flash both boards
3. Install optocoupler isolation circuit
4. Connect P1 connector via optocouplers to Nano
5. Monitor SSR behavior via web interface and TFT display

## Safety Considerations

1. **Always use optocouplers** for electrical isolation in high-power relay applications
2. **Test relay commands** before connecting to actual SSRs/heating elements
3. **Monitor serial handshake** during initial setup to confirm baud rate match
4. **Implement watchdog** on Nano side to shut down relays on communication loss (already implemented)

## References

- ESP32-2432S028R Schematic: [CYD Hardware](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- 6N137 Datasheet: High-speed optocoupler
- MAX6675 Datasheet: K-type thermocouple amplifier
