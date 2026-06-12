# VL53L0X

## World smallest Time-of-Flight ranging and gesture detection sensor

---

### Features

- Fully integrated miniature module
  - 940nm Laser VCSEL
  - VCSEL driver
  - Ranging sensor with advanced embedded micro controller
  - 4.4 x 2.4 x 1.0mm
- Fast, accurate distance ranging
  - Measures absolute range up to 2m
  - Reported range is independent of the target reflectance
  - Operates in high infrared ambient light levels
  - Advanced embedded optical cross-talk compensation to simplify cover glass selection
- Eye safe
  - Class 1 laser device compliant with latest standard IEC 60825-1:2014-3rd edition
- Easy integration
  - Single reflowable component
  - No additional optics
  - Single power supply
  - I2C interface for device control and data transfer
  - Xshutdown (Reset) and interrupt GPIO
  - Programmable I2C address

### Applications

- User detection for Personal Computers/Laptops/Tablets and IoT (Energy saving)
- Robotics (obstacle detection)
- White goods (hand detection in automatic faucets, soap dispensers etc...)
- 1D gesture recognition
- Laser assisted Auto-Focus. Enhances and speeds-up camera AF system performance especially in difficult scenes (low light levels, low contrast) or fast moving video mode

### Description

The VL53L0X is a new generation Time-of-Flight (ToF) laser-ranging module housed in the smallest package on the market today, providing accurate distance measurement whatever the target reflectances unlike conventional technologies. It can measure absolute distances up to 2m, setting a new benchmark in ranging performance levels, opening the door to various new applications.

The VL53L0X integrates a leading-edge SPAD array (Single Photon Avalanche Diodes) and embeds ST's second generation FlightSense™ patented technology.

The VL53L0X's 940nm VCSEL emitter (Vertical Cavity Surface-Emitting Laser), is totally invisible to the human eye, coupled with internal physical infrared filters, it enables longer ranging distance, higher immunity to ambient light and better robustness to cover-glass optical cross-talk.

*May 2016 - This is information on a product in full production.*

*DocID029104 Rev 1*

---

## Contents

### Overview

1.1 Technical specification  
1.2 System block diagram  
1.3 Device pinout  
1.4 Application schematic  

### 2 Functional description

2.1 System functional description  
2.2 Firmware state machine description  
2.3 Customer manufacturing calibration flow  
2.3.1 SPAD and temperature calibration  
2.3.2 Ranging offset calibration  
2.3.3 Cross-talk calibration  
2.4 Ranging operating modes  
2.5 Ranging profiles  
2.6 Ranging profile phases  
2.6.1 Initialization and load calibration data phase  
2.6.2 Ranging phase  
2.6.3 Digital housekeeping  
2.7 Getting the data: interrupt or polling  
2.8 Device programming and control  
2.9 Power sequence  
2.9.1 Power up and boot sequence  
2.10 Ranging sequence  

### 3 Control interface

3.1 I2C interface - timing characteristics  
3.2 I2C interface - reference registers  

### 4 Electrical characteristics

4.1 Absolute maximum ratings  
4.2 Recommended operating conditions  
4.3 ESD  
4.4 Current consumption  
4.5 Electrical characteristics  

### 5 Performance

5.1 Measurement conditions  
5.2 Max ranging distance  
5.3 Ranging accuracy  
5.3.1 Standard deviation  
5.3.2 Range profile examples  
5.3.3 Ranging offset error  

### 6 Outline drawing

### 7 Laser safety considerations

### 8 Packaging and labeling

8.1 Product marking  
8.2 Inner box labeling  
8.3 Packing  
8.3.1 Tape outline drawings  
8.4 Pb-free solder reflow process  
8.5 Handling and storage precautions  
8.5.1 Shock precaution  
8.5.2 Part handling  
8.5.3 Compression force  
8.5.4 Moisture sensitivity level  
8.6 Storage temperature conditions  

### 9 Ordering information

### 10 Acronyms and abbreviations

### 11 ECOPACK®

### 12 Revision history

---

## List of tables

| Table | Title |
|-------|-------|
| Table 1 | Technical specification |
| Table 2 | VL53L0X pin description |
| Table 3 | I2C interface - timing characteristics |
| Table 4 | Reference registers |
| Table 5 | 32-bit register example |
| Table 6 | Absolute maximum ratings |
| Table 7 | Recommended operating conditions |
| Table 8 | ESD performances |
| Table 9 | Consumption at ambient temperature |
| Table 10 | Digital I/O electrical characteristics |
| Table 11 | Max ranging capabilities with 33ms timing budget |
| Table 12 | Ranging accuracy |
| Table 13 | Range profiles |
| Table 14 | Ranging offset |
| Table 15 | Recommended solder profile |
| Table 16 | Recommended storage conditions |
| Table 17 | Ordering information |
| Table 18 | Acronyms and abbreviations |
| Table 19 | Document revision history |

---

## List of figures

| Figure | Title |
|--------|-------|
| Figure 1 | VL53L0X block diagram |
| Figure 2 | VL53L0X pinout (bottom view) |
| Figure 3 | VL53L0X schematic |
| Figure 4 | VL53L0X system functional description |
| Figure 5 | Firmware state machine |
| Figure 6 | Customer manufacturing calibration flow |
| Figure 7 | Range offset |
| Figure 8 | Cross-talk compensation |
| Figure 9 | Typical initialization / ranging / housekeeping phases |
| Figure 10 | Power up and boot sequence |
| Figure 11 | Power up and boot sequence with XSHUT not controlled |
| Figure 12 | Ranging sequence |
| Figure 13 | Data transfer protocol |
| Figure 14 | VL53L0X I2C device address: 0x52 |
| Figure 15 | VL53L0X data format (write) |
| Figure 16 | VL53L0X data format (read) |
| Figure 17 | VL53L0X data format (sequential write) |
| Figure 18 | VL53L0X data format (sequential read) |
| Figure 19 | I2C timing characteristics |
| Figure 20 | Typical ranging (default mode) |
| Figure 21 | Typical ranging - Long range mode |
| Figure 22 | Outline drawing (page 1/3) |
| Figure 23 | Outline drawing (page 2/3) |
| Figure 24 | Outline drawing - with liner (page 3/3) |
| Figure 25 | Class 1 laser product label |
| Figure 26 | Example of marking |
| Figure 27 | Tape outline drawing |
| Figure 28 | Solder profile |

---

# 1 Overview

## 1.1 Technical specification

**Table 1. Technical specification**

| Feature | Detail |
|---------|--------|
| Package | Optical LGA12 |
| Size | 4.40 x 2.40 x 1.00 mm |
| Operating voltage | 2.6 to 3.5 V |
| Operating temperature | -20 to 70°C |
| Infrared emitter | 940 nm |
| I2C | Up to 400 kHz (FAST mode) serial bus |
| Address | 0x52 |

## 1.2 System block diagram

*[Figure 1: VL53L0X block diagram]*

The block diagram shows the VL53L0X module containing:
- VL53L0X silicon with:
  - Detection array (Single Photon Avalanche Diode - SPAD)
  - Non Volatile Memory
  - ROM
  - RAM
  - Microcontroller
  - Advanced Ranging Core
  - VCSEL Driver
- External connections: GND, SDA, SCL, AVSSVCSEL, AVDD, XSHUT, GPIO1, AVDDVCSEL
- IR+ and IR- connections to 940nm VCSEL emitter

## 1.3 Device pinout

*[Figure 2: VL53L0X pinout (bottom view)]*

The pinout shows a 12-pin LGA package with the following arrangement (bottom view):

| Pin number | Signal name | Signal type | Signal description |
|------------|-------------|-------------|-------------------|
| 1 | AVDDVCSEL | Supply | VCSEL Supply, to be connected to main supply |
| 2 | AVSSVCSEL | Ground | VCSEL Ground, to be connected to main ground |
| 3 | GND | Ground | To be connected to main ground |
| 4 | GND2 | Ground | To be connected to main ground |
| 5 | XSHUT | Digital input | Xshutdown pin, Active LOW |
| 6 | GND3 | Ground | To be connected to main ground |
| 7 | GPIO1 | Digital output | Interrupt output. Open drain output. |
| 8 | DNC | Digital input | Do Not Connect, must be left floating. |
| 9 | SDA | Digital input/output | I2C serial data |
| 10 | SCL | Digital input | I2C serial clock input |
| 11 | AVDD | Supply | Supply, to be connected to main supply |
| 12 | GND4 | Ground | To be connected to main ground |

## 1.4 Application schematic

*[Figure 3: VL53L0X schematic]*

The application schematic shows:
- HOST connected to XSHUT (pin 5), GPIO1 (pin 7), SDA (pin 9), SCL (pin 10)
- DNC (pin 8) left unconnected
- AVDDVCSEL (pin 1) and AVDD (pin 11) connected to main supply with 100nF and 4.7μF decoupling capacitors
- AVSSVCSEL (pin 2), GND (pin 3), GND2 (pin 4), GND3 (pin 6), GND4 (pin 12) connected to main ground
- IOVDD supply for pull-up resistors on I2C lines, XSHUT, and GPIO1

**Notes:**
- Capacitors on external supply AVDD should be placed as close as possible to the AVDDVCSEL and AVSSVCSEL module pins.
- External pull-up resistors values can be found in I2C-bus specification. Pull-ups are typically fitted only once per bus, near the host.
- Recommended values for pull-up resistors for an AVDD of 2.8V and 400KHz I2C clock would be 1.5k to 2k Ohms.
- XSHUT pin must always be driven to avoid leakage current. Pull-up is needed if the host state is not known.
- XSHUT is needed to use HW standby mode (no I2C comm).
- XSHUT and GPIO1 pull up recommended values are 10k Ohms.
- GPIO1 to be left unconnected if not used.

---

# 2 Functional description

## 2.1 System functional description

*[Figure 4: VL53L0X system functional description]*

The host customer application controls the VL53L0X device using an API (Application Programming Interface). The API exposes a set of high level functions that allow control of the VL53L0X Firmware (FW) like initialization/calibration, ranging Start/Stop, choice of accuracy, choice of ranging mode.

The API is a turnkey solution consisting of a set of C functions which enables fast development of end user applications, without the complication of direct multiple register access. The API is structured to be compiled on any kind of platform through a well isolated platform layer.

The API package allows the user to take full benefit of VL53L0X capabilities. A detailed description of the API is available in the VL53L0X API User Manual (separate document, DocID029105).

VL53L0X FW fully manages the hardware (HW) register accesses.

## 2.2 Firmware state machine description

*[Figure 5: Firmware state machine]*

The firmware state machine includes the following states and transitions:

- **Power Off** → **Hw Standby**: Host applies AVDD
- **Hw Standby** → **Fw Initial Boot**: Host raises XSHUT
- **Fw Initial Boot** → **Sw Standby**: Automatic transition
- **Sw Standby** → **Range Mode**: Host initiates START
- **Range Mode** branches to:
  - **Continuous Timed** → Range Meas → Inter-Meas Standby → (loop back if inter-measurement period not completed)
  - **Continuous** → Range Meas → (next start starts automatically after the last has finished)
  - **Single** → Range Meas → (returns to Sw Standby)
- **Range Mode** → **Sw Standby**: Host initiates STOP (or automatic move to SW standby)
- **Sw Standby** → **Hw Standby**: Host lowers XSHUT
- **Hw Standby** → **Power Off**: Host removes AVDD

## 2.3 Customer manufacturing calibration flow

*[Figure 6: Customer manufacturing calibration flow]*

The recommended calibration flow at customer level (factory, once only):

1. **Device initialization and settings** (~40ms*) - Initialization, to be called once after device reset
2. **SPADs calibration** (~10ms*) - Initial calibration, required only once, host to store values
3. **Temperature calibration** (~40ms*) - Calibration step to be repeated with > 8 degreeC temperature change
4. **Offset calibration** (~300ms*) - Initial calibration, required only once, host to store values
5. **CrossTalk calibration** (~1sec*) - Initial calibration, required only once, host to store values

*\* Timings are given for information only, they can vary depending on the Host capabilities*

### 2.3.1 SPAD and temperature calibration

In order to optimize the dynamic of the system, the reference SPADs have to be calibrated. Reference SPAD calibration needs to be done only once during the initial manufacturing calibration; the calibration data should then be stored on the host.

Temperature calibration is the calibration of two parameters (VHV and phase cal) which are temperature dependent. These two parameters are used to set the device sensitivity. Calibration should be performed during initial manufacturing calibration; it must be performed again when temperature varies more than 8°C compared to the initial calibration temperature.

For more details on SPAD and temperature calibration please refer to the VL53L0X API User Manual.

### 2.3.2 Ranging offset calibration

Ranging offset can be characterized by the mean offset, which is the centering of the measurement versus the real distance. Offset calibration should be performed at factory for optimal performances (recommended at 10cm). The offset calibration should take into account:
- Supply voltage and temperature
- Protective cover glass above VL53L0X module

*[Figure 7: Range offset]*

The figure shows measured range vs actual range, with p2p_offset calibration shifting the curve to align with ideal 1:1 correlation.

### 2.3.3 Cross-talk calibration

Cross-talk is defined as the signal return from the cover glass. The magnitude of the crosstalk depends on the type of glass and air gap. Cross-talk results in a range error which is proportional to the ratio of the cross-talk to the signal return from the target.

*[Figure 8: Cross-talk compensation]*

The figure shows measured range vs actual range, demonstrating how cross-talk compensation corrects the non-linear error introduced by cover glass reflections.

Full offset and cross-talk calibration procedure is described in the VL53L0X API User Manual.

## 2.4 Ranging operating modes

There are 3 ranging modes available in the API:

1. **Single ranging**: Ranging is performed only once after the API function is called. System returns to SW standby automatically.
2. **Continuous ranging**: Ranging is performed in a continuous way after the API function is called. As soon as the measurement is finished, another one is started without delay. User has to stop the ranging to return to SW standby. The last measurement is completed before stopping.
3. **Timed ranging**: Ranging is performed in a continuous way after the API function is called. When a measurement is finished, another one is started after a user defined delay. This delay (inter-measurement period) can be defined through the API. User has to stop the ranging to return to SW standby. If the stop request comes during a range measurement, the measurement is completed before stopping. If it happens during an inter-measurement period, the range measurement stops immediately.

## 2.5 Ranging profiles

There are 4 different ranging profiles available via API example code. Customers can create their own ranging profile dependent on their use case performance requirements. For more details please refer to the VL53L0X API User Manual.

1. Default mode
2. High speed
3. High accuracy
4. Long range

## 2.6 Ranging profile phases

Each range profile consists of 3 consecutive phases:
- Initialization and load calibration data
- Ranging
- Digital housekeeping

*[Figure 9: Typical initialization / ranging / housekeeping phases]*

### 2.6.1 Initialization and load calibration data phase

Initialization and calibration phase is performed before the first ranging or after a device reset. The user may then have to repeat the temperature calibration phase in a periodic way, depending on the use case.

For more details on the calibration functions please refer to the VL53L0X API User Manual.

### 2.6.2 Ranging phase

The ranging phase consists of a range setup then range measurement.

During the ranging operation, several VCSEL infrared pulses are emitted, then reflected back by the target object, and detected by the receiving array. The photo detector used inside VL53L0X is using advanced ultra-fast SPAD technology (Single Photon Avalanche Diodes), protected by several integrated optical infrared filters.

The typical timing budget for a range is 33ms (init/ranging/housekeeping), with the actual range measurement taking 23ms. The minimum range measurement period is 20ms.

**Note:** The minimum range timing budget is 20ms. Maximum is 5 seconds. The longer the timing budget, the higher the accuracy and the ranging distance.

### 2.6.3 Digital housekeeping

Digital processing (housekeeping) is the last operation inside the ranging sequence that computes, validates or rejects a ranging measurement. Part of this processing is performed internally while the other part is executed on the Host by the API.

At the end of the digital processing, the ranging distance is computed by VL53L0X itself. If the distance could not be measured (weak signal, no target...), a corresponding error code is returned.

The following functions are performed on the device itself:
- Signal value check (weak signal)
- Offset correction
- Cross-talk correction (in case of cover glass)
- Final ranging value computation

While the API performs the following:
- Return Ignore Threshold RIT check (Signal check versus cross talk)
- Sigma check (accuracy condition)
- Final ranging state computation

If the user wants to enhance the ranging accuracy, some extra processing (not part of the API) can be carried out by the host, for example, rolling average, hysteresis or any kind of filtering.

## 2.7 Getting the data: interrupt or polling

User can get the final data using a polling or an interrupt mode.

- **Polling mode**: user has to check the status of the ongoing measurement by polling an API function.
- **Interrupt mode**: An interrupt pin (GPIO1) sends an interrupt to the host when a new measurement is ready.

The description of these 2 modes is available in the VL53L0X API User Manual.

## 2.8 Device programming and control

Device physical control interface is I2C, described in Section 3: Control interface.

A software layer (API) is provided to control the device. The API is described in the VL53L0X API User Manual.

## 2.9 Power sequence

### 2.9.1 Power up and boot sequence

There are two options available for device power up/boot.

**Option 1: XSHUT pin connected and controlled from host.**

This option helps to optimize power consumption as the VL53L0X can be completely powered off when not used, and then woken up through host GPIO (using XSHUT pin).

HW Standby mode is defined as the period when AVDD is present and XSHUT is low.

*[Figure 10: Power up and boot sequence]*

Sequence: OFF → HW STANDBY (AVDD applied) → FW BOOT (XSHUT raised) → SW STANDBY

tBOOT is 1.2ms max.

**Option 2: XSHUT pin not controlled by host, and tied to AVDD through pull-up resistor.**

In case XSHUT pin is not controlled, the power up sequence is presented in Figure 11. In this case, the device is going automatically in SW STANDBY after FW BOOT, without entering HW STANDBY.

*[Figure 11: Power up and boot sequence with XSHUT not controlled]*

Sequence: OFF → FW BOOT (AVDD applied, XSHUT tied high) → SW STANDBY

tBOOT is 1.2ms max.

## 2.10 Ranging sequence

*[Figure 12: Ranging sequence]*

The ranging sequence shows:
- AVDD and XSHUT remain high
- I2C communication for API INIT (in SW STANDBY)
- API START RANGING command initiates the sequence
- System enters Init/ranging/housekeeping phase
- GPIO1 generates interrupt when measurement complete
- API GET RANGING retrieves data
- System returns to SW STANDBY

t_timing_budget is a parameter set by the user, using a dedicated API function. Default value is 33ms.

---

# 3 Control interface

This section specifies the control interface. The I2C interface uses two signals: serial data line (SDA) and serial clock line (SCL). Each device connected to the bus is using a unique address and a simple master/slave relationship exists.

Both SDA and SCL lines are connected to a positive supply voltage using pull-up resistors located on the host. Lines are only actively driven low. A high condition occurs when lines are floating and the pull-up resistors pull lines up. When no data is transmitted both lines are high.

Clock signal (SCL) generation is performed by the master device. The master device initiates data transfer. The I2C bus on the VL53L0X has a maximum speed of 400kbits/s and uses a device address of 0x52.

*[Figure 13: Data transfer protocol]*

Information is packed in 8-bit packets (bytes) always followed by an acknowledge bit, Ac for VL53L0X acknowledge and Am for master acknowledge (host bus master). The internal data is produced by sampling SDA at a rising edge of SCL. The external data must be stable during the high period of SCL. The exceptions to this are start (S) or stop (P) conditions when SDA falls or rises respectively, while SCL is high.

A message contains a series of bytes preceded by a start condition and followed by either a stop or repeated start (another start condition but without a preceding stop condition) followed by another message. The first byte contains the device address (0x52) and also specifies the data direction. If the least significant bit is low (that is, 0x52) the message is a master write to the slave. If the lsb is set (that is, 0x53) then the message is a master read from the slave.

*[Figure 14: VL53L0X I2C device address: 0x52]*

| MSBit | | | | | | | LSBit |
|-------|---|---|---|---|---|---|-------|
| 0 | 1 | 0 | 1 | 0 | 0 | 1 | R/W |

All serial interface communications with the camera module must begin with a start condition. The VL53L0X module acknowledges the receipt of a valid address by driving the SDA wire low. The state of the read/write bit (lsb of the address byte) is stored and the next byte of data, sampled from SDA, can be interpreted. During a write sequence the second byte received provides an 8-bit index which points to one of the internal 8-bit registers.

*[Figure 15: VL53L0X data format (write)]*

Sequence: Start → ADDRESS[7:0] (0x52 write) → Ac (VL53L0X acknowledges valid address) → INDEX[7:0] → Ac → DATA[7:0] → Ac → Stop

As data is received by the slave it is written bit by bit to a serial/parallel register. After each data byte has been received by the slave, an acknowledge is generated, the data is then stored in the internal register addressed by the current index.

During a read message, the contents of the register addressed by the current index is read out in the byte following the device address byte. The contents of this register are parallel loaded into the serial/parallel register and clocked out of the device by the falling edge of SCL.

*[Figure 16: VL53L0X data format (read)]*

Write phase: Start → ADDRESS[7:0] (0x52) → Ac → INDEX[7:0] → Ac → Stop  
Read phase: Start → ADDRESS[7:0] (0x53) → Ac → DATA[7:0] → Am (Master acknowledge) → Stop

At the end of each byte, in both read and write message sequences, an acknowledge is issued by the receiving device (that is, the VL53L0X for a write and the host for a read).

A message can only be terminated by the bus master, either by issuing a stop condition or by a negative acknowledge (that is, not pulling the SDA line low) after reading a complete byte during a read sequence.

The interface also supports auto-increment indexing. After the first data byte has been transferred, the index is automatically incremented by 1. The master can therefore send data bytes continuously to the slave until the slave fails to provide an acknowledge or the master terminates the write communication with a stop condition. If the auto-increment feature is used the master does not have to send address indexes to accompany the data bytes.

*[Figure 17: VL53L0X data format (sequential write)]*

Sequence: Start → ADDRESS[7:0] (0x52 write) → Ac → INDEX[7:0] → Ac → DATA[7:0] → Ac → DATA[7:0] → Ac → DATA[7:0] → Ac → Stop

*[Figure 18: VL53L0X data format (sequential read)]*

Write phase: Start → ADDRESS[7:0] (0x52) → Ac → INDEX[7:0] → Ac → Stop  
Read phase: Start → ADDRESS[7:0] (0x53) → Ac → DATA[7:0] → Am → DATA[7:0] → Am → DATA[7:0] → Am → DATA[7:0] → Am → DATA[7:0] → Am → Stop

## 3.1 I2C interface - timing characteristics

Timing characteristics are shown in Table 3. Please refer to Figure 19 for an explanation of the parameters used. Timings are given for all PVT conditions.

**Table 3. I2C interface - timing characteristics**

| Symbol | Parameter | Minimum | Typical | Maximum | Unit |
|--------|-----------|---------|---------|---------|------|
| FI2C | Operating frequency (Standard and Fast mode) | 0 | - | 400(1) | kHz |
| tLOW | Clock pulse width low | 1.3 | - | - | μs |
| tHIGH | Clock pulse width high | 0.6 | - | - | μs |
| tSP | Pulse width of spikes which are suppressed by the input filter | - | - | 50 | ns |
| tBUF | Bus free time between transmissions | 1.3 | - | - | ms |
| tHD.STA | Start hold time | 0.26 | - | - | μs |
| tSU.STA | Start set-up time | 0.26 | - | - | μs |
| tHD.DAT | Data in hold time | 0 | - | 0.9 | μs |
| tSU.DAT | Data in set-up time | 50 | - | - | ns |
| tR | SCL/SDA rise time | - | - | 120 | ns |
| tF | SCL/SDA fall time | - | - | 120 | ns |
| tSU.STO | Stop set-up time | 0.6 | - | - | μs |
| Ci/o | Input/output capacitance (SDA) | - | - | 10 | pF |
| Cin | Input capacitance (SCL) | - | - | 4 | pF |
| CL | Load capacitance | - | 125 | 400 | pF |

*(1) The maximum bus speed is also limited by the combination of 400pF load capacitance and pull-up resistor. Please refer to the I2C specification for further information.*

*[Figure 19: I2C timing characteristics]*

The figure illustrates the timing relationships between SDA and SCL signals including:
- Start condition (tHD.STA, tSU.STA)
- Stop condition (tSU.STO)
- Data setup (tSU.DAT) and hold (tHD.DAT) times
- Clock high (tHIGH) and low (tLOW) periods
- Bus free time (tBUF)
- Rise (tR) and fall (tF) times

All timings are measured from either VIL or VIH.

## 3.2 I2C interface - reference registers

The registers shown in the table below can be used to validate the user I2C interface.

**Table 4. Reference registers**

| Address (after fresh reset, without API loaded) | Value |
|------------------------------------------------|-------|
| 0xC0 | 0xEE |
| 0xC1 | 0xAA |
| 0xC2 | 0x10 |
| 0x51 | 0x0099 |
| 0x61 | 0x0000 |

**Note:** I2C read/writes can be 8, 16 or 32-bit. Multi-byte reads/writes are always addressed in ascending order with MSB first as shown in Table 5.

**Table 5. 32-bit register example**

| Register address | Byte |
|-----------------|------|
| Address | MSB |
| Address + 1 | .. |
| Address + 2 | .. |
| Address + 3 | LSB |

---

# 4 Electrical characteristics

## 4.1 Absolute maximum ratings

**Table 6. Absolute maximum ratings**

| Parameter | Min. | Typ. | Max. | Unit |
|-----------|------|------|------|------|
| AVDD | -0.5 | - | 3.6 | V |
| SCL, SDA, XSHUT and GPIO1 | -0.5 | - | 3.6 | V |

**Note:** Stresses above those listed in Table 6 may cause permanent damage to the device. This is a stress rating only and functional operation of the device at these or any other conditions above those indicated in the operational sections of the specification is not implied. Exposure to absolute maximum rating conditions for extended periods may affect device reliability.

## 4.2 Recommended operating conditions

**Table 7. Recommended operating conditions(1)**

| Parameter | Min. | Typ. | Max. | Unit |
|-----------|------|------|------|------|
| Voltage (AVDD) | 2.6 | 2.8 | 3.5 | V |
| IO (IOVDD) - Standard mode | 1.6 | 1.8 | 1.9 | V |
| IO (IOVDD) - 2V8 mode(3)(4) | 2.6 | 2.8 | 3.5 | V |
| Temperature (normal operating) | -20 | +25 | +70 | °C |

*(1) There are no power supply sequencing requirements. The I/Os may be high, low or floating when AVDD is applied. The I/Os are internally failsafe with no diode connecting them to AVDD.*

*(2) XSHUT should be high level only when AVDD is on.*

*(3) SDA, SCL, XSHUT and GPIO1 high levels have to be equal to AVDD in 2V8 mode.*

*(4) The default API mode is 1V8. 2V8 mode is programmable using device settings loaded by the API. For more details please refer to the VL53L0X API User Manual.*

## 4.3 ESD

VL53L0X is compliant with ESD values presented in Table 8.

**Table 8. ESD performances**

| Parameter | Specification | Conditions |
|-----------|--------------|------------|
| Human Body Model | JS-001-2012 | +/- 2kV, 1500 Ohms, 100pF |
| Charged Device Model | JZSD22-C101 | +/- 500V |

## 4.4 Current consumption

**Table 9. Consumption at ambient temperature(1)**

| Parameter | Min. | Typ. | Max. | Unit |
|-----------|------|------|------|------|
| HW STANDBY | 3 | 5 | 7 | μA |
| SW STANDBY (2V8 mode)(2) | 4 | 6 | 9 | μA |
| Timed ranging Inter measurement | - | 16 | 19 | μA |
| Active Ranging average consumption (including VCSEL) (3)(4) | - | - | - | mA |
| Average power consumption at 10Hz with 33ms ranging sequence | - | 20 | - | mW |

*(1) All current consumption values include silicon process variations. Temperature and Voltage are at nominal conditions (23°C and 2.8V). All values include AVDD and AVDDVCSEL.*

*(2) In standard mode (1V8), pull-ups have to be modified, then SW STANDBY consumption is increased by +0.6μA.*

*(3) Active ranging is an average value, measured using default API settings (33ms timing budget).*

*(4) Peak current (including VCSEL) can reach [value not specified in extracted text].*

## 4.5 Electrical characteristics

**Table 10. Digital I/O electrical characteristics**

| Symbol | Parameter | Minimum | Typical | Maximum | Unit |
|--------|-----------|---------|---------|---------|------|
| **Interrupt pin (GPIO1)** |
| VIL | Low level input voltage | - | - | 0.3 IOVDD | V |
| VIH | High level input voltage | 0.7 IOVDD | - | - | V |
| VOL | Low level output voltage (IOUT = 4 mA) | - | - | 0.4 | V |
| VOH | High level output voltage at (IOUT = 4 mA) | IOVDD-0.4 | - | - | V |
| FGPIO | Operating frequency (CLOAD = 20 pF) | 0 | - | 108 | MHz |
| **I2C interface (SDA/SCL)** |
| VIL | Low level input voltage | -0.5 | - | 0.6 | V |
| VIH | High level input voltage | 1.12 | - | IOVDD+0.5 | V |
| VOL | Low level output voltage (IOUT = 4 mA in Standard and Fast modes) | - | - | 0.4 | V |
| IIL/IIH | Leakage current(1) | - | - | 10 | μA |
| IIL/IIH | Leakage current(2) | - | - | 0.15 | μA |

*(1) AVDD = 0V*  
*(2) AVDD = 2.85V; I/O voltage = 1.8V*

---

# 5 Performance

## 5.1 Measurement conditions

In all measurement tables in the document, it is considered that the full Field Of View (FOV) is covered.

VL53L0X system FOV is 25 degrees. Reflectance targets are standard ones (Grey 17% N4.74 and White 88% N9.5 Munsell).

Unless mentioned, device is controlled through the API using the default settings (refer to VL53L0X API User Manual for API settings).

*[Figure 20: Typical ranging (default mode)]*

The figure shows API_RangeValue (mm) vs. Actual Target Distance (mm) for grey17 and white88 reflectance targets in default mode.

## 5.2 Max ranging distance

*[Figure 21: Typical ranging - Long range mode]*

The figure shows API_RangeValue (mm) vs. Actual Target Distance (mm) for grey17 and white88 reflectance targets in long range mode.

**Table 11. Max ranging capabilities with 33ms timing budget**

| Target reflectance level (Full FOV) | Conditions | Indoor(2) | Outdoor overcast(2) |
|-------------------------------------|------------|-----------|---------------------|
| **White Target (88%)** |
| Typical | | 200cm+(1) | 80cm |
| Minimum | | 120cm | 60cm |
| **Grey Target (17%)** |
| Typical | | 80cm | 50cm |
| Minimum | | 70cm | 40cm |

*(1) Using long range API profile*

*(2) Indoor: no infrared. Outdoor overcast corresponds to a parasitic noise of 10kcps/SPAD for VL53L0X module. For reference, this corresponds to a 1.2W/m² at 940nm, and is equivalent to 5kLux daylight, while ranging on a grey 17% chart at 40cm.*

**Measurement conditions:**
- Targets reflectance used: Grey (17%), White (88%)
- Nominal Voltage (2.8V) and Temperature (23°C)
- All distances are for a complete Field of View covered (FOV = 25 degrees)
- 33ms timing budget

All distances mentioned in this table are guaranteed for a minimum detection rate of 94% (up to 100%). Detection rate is the worst case percentage of measurements that will return a valid measurement when target is present.

## 5.3 Ranging accuracy

### 5.3.1 Standard deviation

Ranging accuracy can be characterized by standard deviation. It includes Measure-to-Measure and Part-to-Part (silicon) variations.

**Table 12. Ranging accuracy**

| Target reflectance level (Full FOV) | Distance | Indoor (no infrared) | | Outdoor | |
|-------------------------------------|----------|----------------------|---|---------|---|
| | | 33ms | 66ms | 33ms | 66ms |
| White Target (88%) | at 120cm | 4% | 3% | at 60cm | 7% | 6% |
| Grey Target (17%) | at 70cm | 7% | 6% | at 40cm | 12% | 9% |

**Measurement conditions:**
- Targets reflectance used: Grey (17%), White (88%)
- Offset correction done at 10cm from sensor
- Indoor: no Infrared / Outdoor: eq. 5kLux equivalent sunlight (10kcps/SPAD)
- Nominal Voltage (2v8) and Temperature (23°C)
- All distances are for a complete Field of View covered (FOV = 25 degrees)
- Detection rate is considered at 94% minimum

### 5.3.2 Range profile examples

**Table 13. Range profiles**

| Range Profile | Range timing budget | Typical performance | Typical application |
|---------------|--------------------|---------------------|---------------------|
| Default mode | 30ms | 1.2m, accuracy as per Table 12 | standard |
| High accuracy | 200ms | 1.2m, accuracy < +/- 3% | precise measurement |
| Long range | 33ms | 2m, accuracy as per Table 12 | long ranging, only for dark conditions (no IR) |
| High speed | 20ms | 1.2m, accuracy +/- 5% | high speed where accuracy is not priority |

### 5.3.3 Ranging offset error

The table below shows how range offset may drift over distance, voltage and temperature. Assumes offset calibrated at 10cm. See VL53L0X API User Manual for details on offset calibration.

**Table 14. Ranging offset**

| Nominal Conditions | Measure point | Typical offset from nominal | Maximum offset from nominal |
|-------------------|---------------|----------------------------|----------------------------|
| **Ranging distance** - Offset calibration at 10cm ("zero") |
| | White 120cm (indoor) | < 3% | |
| | Grey 70cm (indoor) | | |
| | White 60cm (outdoor) | | |
| | Grey 40cm (outdoor) | | |
| **Voltage drift** - 2.8V | 2.6V to 3.5V | +/- 10mm | +/- 15mm |
| **Temperature drift** - 23°C | -20°C to +70°C | +/- 10mm | +/- 30mm |

---

# 6 Outline drawing

*[Figure 22: Outline drawing (page 1/3)]*

The outline drawing shows:
- Module dimensions: 4.40 ± 0.05 mm (length) x 2.40 ± 0.05 mm (width) x 1.00 mm (height, with 0.665mm main body + 0.15-0.10mm lid)
- Pin 1 marking location
- Aperture dimensions: Ø0.40 (emitter), Ø0.20 (detector)
- Pad layout and dimensions
- PWB solder pattern with 0.80 x 0.80 mm pads
- Connection table showing all 12 pads and their functions

*[Figure 23: Outline drawing (page 2/3)]*

The outline drawing shows:
- 3D isometric views of the module
- Emitter exclusion cone: 35° full angle, Ø0.40 at Datum A
- Collector exclusion cone: 25° full angle, Ø0.20 at Datum A
- Top and side views showing optical component placement

*[Figure 24: Outline drawing - with liner (page 3/3)]*

The outline drawing shows:
- Delivered configuration with protective liner
- Module with liner dimensions: 2.92 mm width x 4.0 mm length
- Liner overhang: 0.20 ± 0.20 mm on sides, 0.15 ± 0.15 mm on ends
- Corner radius: R0.20 (in 4 positions)
- Liner thickness: 0.046 mm
- Module height with liner: 1.05 mm

---

# 7 Laser safety considerations

The VL53L0X contains a laser emitter and corresponding drive circuitry. The laser output is designed to remain within Class 1 laser safety limits under all reasonably foreseeable conditions including single faults in compliance with IEC 60825-1:2014 (third edition).

The laser output will remain within Class 1 limits as long as the STMicroelectronics recommended device settings (API settings) are used and the operating conditions specified are respected.

**Caution:**

The laser output power must not be increased by any means and no optics should be used with the intention of focusing the laser beam.

Use of controls or adjustments or performance of procedures other than those specified herein may result in hazardous radiation exposure.

*[Figure 25: Class 1 laser product label]*

---

# 8 Packaging and labeling

## 8.1 Product marking

A 2-line product marking is applied on the backside of the module (i.e. on the substrate). The first line is the silicon product code, and the second line, the internal tracking code.

*[Figure 26: Example of marking]*

The image shows the bottom view of the module with marking "VL53L0B" (first line) and "K08560A" (second line, internal tracking code).

## 8.2 Inner box labeling

The labeling follows the ST standard packing acceptance criteria.

The following information will be on the inner box label:
- assembly site
- sales type
- quantity
- trace code
- marking
- bulk ID number

## 8.3 Packing

At customer/subcontractor level, it is recommended to mount the VL53L0X in a clean environment to avoid foreign material contamination.

To help avoid any foreign material contamination at phone assembly level the modules will be shipped in a tape and reel format with a protective liner. The packaging will be vacuum sealed and include a desiccant.

The liner is compliant with reflow at 260°C. It must be removed during assembly of the customer device, just before mounting the cover glass.

### 8.3.1 Tape outline drawings

*[Figure 27: Tape outline drawing]*

The tape outline drawing shows:
- Carrier tape dimensions for the LGA12 module
- Pocket dimensions: Ao = 2.70 mm, Bo = 4.80 mm, Ko = 1.30 mm
- Pitch: P1 = 8.00 mm, P2 = 2.00 ± 0.05 mm, Po = 4.0 mm
- Tape width: W = 12.0 +0.3/-0.1 mm
- Sprocket hole diameter: Do = 1.50 +0.1/-0 mm
- Component hole diameter: D1 = 1.0 +0.25/-0 mm
- Material: Conductive Polystyrene
- Surface resistivity: 1×10⁴ < SR < 1×10¹¹ OHMS
- Mold type: Rotary mold

## 8.4 Pb-free solder reflow process

*[Figure 28: Solder profile]*

Figure 28 and Table 15 show the recommended and maximum values for the solder profile. Customers will have to tune the reflow profile depending on the PCB, solder paste and material used.

We expect customers to follow the "recommended" reflow profile, which is specifically tuned for VL53L0X package.

For any reason if a customer must perform a reflow profile which is different from "recommended" one (especially peak > 240°C), this new profile must be qualified by the customer at its own risk. In any case, the profile have to be within the "maximum" profile limit described in Table 15.

**Table 15. Recommended solder profile**

| Parameters | Recommended | Maximum | Units |
|------------|-------------|---------|-------|
| Minimum temperature (TS min) | 130 | 200 | °C |
| Maximum temperature (TS max) | 90-110 | 150 | °C |
| Time ts (TS min to TS max) | 200 | 60 - 120 | seconds |
| Temperature (TL) | 217 | 217 | °C |
| Time (tL) | 55-65 | 55 - 65 | seconds |
| Ramp up | +2 | +3 | °C/second |
| Temperature (Tp-10) | - | - | °C |
| Time (tp-10) | - | - | seconds |
| Ramp up | - | - | °C/second |
| Peak temperature (Tp) | 240 | 260 max | °C |
| Time to peak | 300 | 300 | seconds |
| Ramp down (peak to TL) | -4 | -6 | °C/second |

**Note:** Temperature mentioned in Table 15 is measured at the top of VL53L0X package.

**Note:** The component should be limited to a maximum of 3 passes through this solder profile.

## 8.5 Handling and storage precautions

### 8.5.1 Shock precaution

Proximity sensor modules house numerous internal components that are susceptible to shock damage. If a unit is subject to excessive shock, is dropped onto the floor, or a tray/reel of units is dropped onto the floor, it must be rejected, even if no apparent damage is visible.

### 8.5.2 Part handling

Handling must be done with non-marring ESD safe carbon, plastic, or Teflon tweezers. Ranging modules are susceptible to damage or contamination. A clean assembly process is advised at customer after un-taping the parts, and until a protective cover glass is mounted.

### 8.5.3 Compression force

A maximum compressive load of 25N shall be applied on the module.

### 8.5.4 Moisture sensitivity level

Moisture sensitivity is level 3 (MSL) as described in IPC/JEDEC JSTD-020-C.

## 8.6 Storage temperature conditions

**Table 16. Recommended storage conditions**

| Parameter | Min. | Typ. | Max. | Unit |
|-----------|------|------|------|------|
| Temperature (storage) | -40 | - | +85 | °C |

---

# 9 Ordering information

**Table 17. Ordering information**

| Sales type | Package | Packing |
|------------|---------|---------|
| VL53L0CXV0DH/1 | Optical LGA12 with liner | Tape and reel |

---

# 10 Acronyms and abbreviations

**Table 18. Acronyms and abbreviations**

| Acronym/abbreviation | Definition |
|---------------------|------------|
| ESD | Electrostatic discharge |
| I2C | Inter-integrated circuit (serial bus) |
| NVM | Non volatile memory |
| RIT | Return Ignore Threshold |
| SPAD | Single photon avalanche diode |
| VCSEL | Vertical cavity surface emitting laser |

---

# 11 ECOPACK®

In order to meet environmental requirements, ST offers these devices in different grades of ECOPACK® packages, depending on their level of environmental compliance. ECOPACK® specifications, grade definitions and product status are available at: www.st.com.

ECOPACK® is an ST trademark.

---

# 12 Revision history

**Table 19. Document revision history**

| Date | Revision | Changes |
|------|----------|---------|
| 30-May-2016 | 1.0 | Initial release. |

---

# IMPORTANT NOTICE – PLEASE READ CAREFULLY

STMicroelectronics NV and its subsidiaries ("ST") reserve the right to make changes, corrections, enhancements, modifications, and improvements to ST products and/or to this document at any time without notice. Purchasers should obtain the latest relevant information on ST products before placing orders. ST products are sold pursuant to ST's terms and conditions of sale in place at the time of order acknowledgement.

Purchasers are solely responsible for the choice, selection, and use of ST products and ST assumes no liability for application assistance or the design of Purchasers' products.

No license, express or implied, to any intellectual property right is granted by ST herein.

Resale of ST products with provisions different from the information set forth herein shall void any warranty granted by ST for such product.

Information in this document supersedes and replaces information previously supplied in any prior versions of this document.

© 2016 STMicroelectronics – All rights reserved

*DocID029104 Rev 1*