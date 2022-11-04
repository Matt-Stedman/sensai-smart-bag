# ZMIL Firmware development

# Installation onto prototypes

If your device is going into the field, you should ALWAYS be using a tagged version (see [tags here](https://gitlab.com/sparkmate/zmil/zmil-firmware/-/tags)).
Installataion is handled via the setup scripts, quickly put:

- use `pio run -t upload -e setup_step_1` to install the hardware tests to ensure that every hardware element functions correctly,
- use `pio run -t upload -e setup_step_2` to install the brick tests, initialize the box on Strapi, connect the SIM card on EMnify, and if that all passes we automatically install the main firmware.

If you are testing in the office and you want the latest features stick to the dev branch.

If you are testing specific bricks you can use `pio run -t upload -e testing` and modify the `testing/testing_main.cpp` script as you need.

More details of this procedure can be seen on this [Miro board](https://miro.com/app/board/uXjVOLMBQbc=/?moveToWidget=3458764532713603799&cot=14).

# Core functions and expected operations

You can find more information about the tasks and how they relate in this [Miro board](https://miro.com/app/board/uXjVOLMBQbc=/?moveToWidget=3458764531879934919&cot=14).

## Sniffing

Sniff data from the N2k network and accelerometer and write to SD card.

## Subsample

Subsample the data from the SD card so we keep information while minimizing upload quantities. 

## Uploading

Upload the subsampled data to InfluxDB over celllular 4G.
We also upload system statuses and ping Strapi during this time.

## UX

We interact with the PCB in two ways: lights (for outputs) and the 4G-toggle switch.
There are two lights: the NET_STATUS light and the SYSTEM_STATUS light.

- NET_STATUS is directly from the 4G: if it is on then 4G is at least being attempted. Flashing at certain rates tells us different things, but essentially is only really used for debugging.
- SYSTEM_STATUS is an RGB light: Green/Blue is good, Yellow shows mild issues, Red is horrible, more details below.

## Logging

We have an intelligent logging system that periodically checks the hardware, and whenever we log significant changes (breaking hardware, errors, significant statistics) we save this to a recallable buffer.
This buffer is called every hour so we can see the latest brick statuses, and this information also periodically pings up to Strapi.

# Code structure

`main.cpp` calls the task handler declared in `main_task_handler.h` where we also assign priorities to various tasks.

These tasks are found in the `tasks/` directory and are self-contained tasks that the task-scheduler can call upon whenever there is time to complete the tasks.

The tasks will call upon specific bricks - pieces of firmware specific to specific hardware/low-level functions - found in `bricks/`.
In some cases, it makes sense to initialize part of a brick so it can be used more globally, before defining the rest of the brick. This is only really for the SIMCOM and Firmware/VMU meta bricks as they are so tightly bound, and you can see these initializations in `inits/`.

Config files can be found in `configs`, generally accessible at any level.

This logic means that any level can only call upon it's level or below. A task can call a brick, but a brick can not call a task:

```
src/main.cpp                    ← include/configs/*
↑
include/main_task_handler.h     ← ''
↑
include/tasks/*                 ← ''
↑
include/inits/*                 ← '', (used for select bricks only)
↑
include/bricks/*                ← ''
↑
lib/*                           ← ''
```

**Why no src (.cpp) files?**

[You don't need both anymore!](https://stackoverflow.com/questions/1305947/why-does-c-need-a-separate-header-file#:~:text=The%20answer%20is%20that%20C%2B%2B,everything%20in%20the%20header%20files.) _(if you're careful, and you use `pragma once`)_

# Bricks explanation (classes and namespaces)

In order to keep it very clear which function belongs to which brick we've adopted Namespaces. For example `TimeHandler::setRTCFromEpoch` found in [bricks/time_handler.h](./include/bricks/time_handler.h), which is responsible for setting the time on the Real Time Clock from an epoch time. We have done this entirely for the sake of human readibility, not compilation/performance, etc.

You will find that all bricks will use a PascalCased namespace reflective of the filename. The two exceptions are [accelerometer\_\_c.h](./include/bricks/accelerometer__c.h) and [optimized_subsampler\_\_c.h](./include/bricks/optimized_subsampler__c.h). These two bricks are classes, not namespaces.

Namespaces act like objects (from a human readibility perspective) and so are essentially treated as non-replicable (i.e. we only have one `FileHandler` brick, only one `UploadsCacher` brick, etc).

In the cases of the Accelerometer and Subsampler we may find a need for multiple accelerometers in future, and so this class is instantiated as an object at runtime.

In the case of the Optimized Subsampler, we create two subsamplers at runtime. One for the working SESSION file, and one for any old sessions we're uploading. This keeps us uploading only the most relevant data.

# Tasks explanation (multi-threading behaviour)

Tasks are part of the [FreeRTOS framework](https://www.freertos.org/index.html), whereby we run multiple tasks on the ESP32 at the same time, and the framework evaluates what can/should be run at any given time.

We use a combination of scheduler-handled tasks and verbosely ordered tasks.

**For verbosely ordered tasks** (such as `c0_sniffAndFillBuffer`) we shouldn't be filling the Upload Buffer with data before we've sniffed data (it not only doesn't make sense to, but it can lead to nasty results), and so we combine these two subtasks under one main task which we instruct to run continuously on a dedicated core.

As such, the safest way to ensure that a sub-task B always happens after a sub-task A is to verbosely tell A to run then B to run in code flow, and not trust a scheduler to handle this.

For example,
In [main_task_handler.h](./include/main_task_handler.h):

```cpp
void Tasks::c0_sniffAndFillBuffer(void *param)
{
    for (;;)
    {
        DataSniffingTasks::sniff(20); // You must sniff
        if (DATA_UPLOAD_ENABLED)
        {
            DataSniffingTasks::setUploadHistoryChanges(&UploadBufferStream); // Then update and previous upload changes
            DataSniffingTasks::fillUploadBuffer(&UploadBufferStream); // Then fill the Upload Buffer
        }
    }
}
```

These sub-tasks can be found in the files in [tasks](./include/tasks/) folder. In the example above, all of the sub-tasks can be found in [data_sniffing.h](./include/tasks/data_sniffing.h) where we then leverage specific code from bricks, for example (simplified code):

```cpp
    void sniff(int time_limit = 3000)
    {
        unsigned long int time_now = millis();
        FileHandler::openFileForAppend(SESSION_FILE_NAME.c_str());
        while (millis() < time_now + (time_limit))
        {
            NMEA2000.ParseMessages();
            Accelerometer.checkAccel();
        }
        FileHandler::closeFileForAppend(SESSION_FILE_NAME.c_str());
    }
```

**For scheduler-handled tasks** (such as `c1_updateRTCFromNetwork`), we want this task to run whenever the Uploader is not busy, and CORE 1 has some availability. We set this to the same priority as `c1_uploadFromUploadBuffer`, and allow the scheduler to determine when it makes sense to run it. We try to update the RTC at the beginning of operations, and if we were not successful we "sleep the task" for 20s and wait to try again later. Once this task has successfully completed we delete it, i.e. we only update the RTC once per session.

# UX/UI, and logging

## Status light behaviour

| Error            | Pattern                                                                              |
| ---------------- | ------------------------------------------------------------------------------------ |
| SD Card Error    | Flash 5 times at 5Hz, then restart the board                                         |
| RTC Error        | Flash 3 times, .5Hz (long light, short off)                                          |
| SIM Card Error   | Flash 6 times, 1Hz (long light, short off) then disable data upload for this session |
| SIM Module Error | Flash 3 times, 1Hz (with long light, short off) then disable data upload             |
| Unknown Error    | Flash indefinitely at 2Hz                                                            |

## Buttons behaviour

We can slide Button 3 (the switch) to deactivate 4G cellular connectivity.


_n.b. the below is implemented but not relevant as we currently don't expose a button that controls this functionality to the user. This, then, is more for debugging_
| Name                       | Pattern          | Behaviour                                                                |
| --------------------------- | ---------------- | ------------------------------------------------------------------------ |
| _Ignore_                    | Less than 200ms  | _Nothing, ignore, likely a shock_                                        |
| System check                | 200ms to 1s      | Perform and print a system check to console, and upload results online   |
| Restart ESP (safely)        | 1s to 4s         | Safely reset the module, powering down the SIM module                    |
| Restart in OTA upgrade mode | 4s to 10s        | Restart the module specifically in OTA upload mode (not yet implemented) |
| _Ignore_                    | More than to 10s | _Nothing, ignore, likely something is leaning on the button_             |

# Testing

There is a dedicated test script in the [testing/testing_main.cpp](testing/testing_main.cpp) file for running tests on hardware, bricks, and tasks. These run specific tests as defined in `include/testing/`. Comment and uncomment the specific tests as you see fit.

To use this `test/testing_main.cpp` over the standard `src/main.cpp` you must run the relevant pio upload command in your terminal (bash/powershell): `pio run -t upload -e testing`. This command can also be found commented on line 36 in the [platformio.ini](./platformio.ini) file that says for reference.

# Data Collection and Parsing Systems

_How do we collect data from the boat's N2k network, and store it somewhere?_

- Leverages the [NMEA2000](https://github.com/ttlappalainen/NMEA2000) and [NMEA2000-esp32](https://github.com/ttlappalainen/NMEA2000_esp32) libraries
- Parses data from the CAN connector into the SD card.
- See [n2k_parser.h](include/bricks/n2k_parser.h) and [file_handler.h](include/bricks/file_handler.h) for these functions.

**Data is stored on the SD card** as text lines in a text file in the format:

```
<TIME (epoch, s)>,<PGN>,<TITLE>,<KEY_1>=<VAL=1>,<KEY_2>=<VAL_2>,...<KEY_N>=<VAL_N>\r\n
```

For example, for the Course Over Ground and Speed Over Ground (COG/SOG) PGNs:

```
1650355763,129026,COGSOG, COG_deg=358.13, SOG_ms=0.00, SID=not available, reference=true
```

For unrecognised/unrecorded PGNs we simply log the time and PGN only:

```
1650355769,126720
```

**We are able to decide which PGNs to store** (and upload) using the `NMEA2000Handlers[]` variable in [n2k_parser.h](include/bricks/n2k_parser.h). For example:

```cpp
tNMEA2000Handler NMEA2000Handlers[] = {
    {126992L, "SysTime", &SystemTime, 1, 0},       // Do log, but don't upload.
    {127258L, "MagVar", &MagneticVariation, 1, 1}, // Upload the first variable only
    {127489L, "EngineDynParams", &EngineDynamicParameters, 1, 3},
    {127497L, "TripFuelConsump", &TripFuelConsumption, 1, 0},
    {129026L, "COGSOG", &COGSOG, 1, 2},
    {129029L, "GNSS", &GNSS, 1, 1},
    {0, "", 0, 0, 0}};
```

The `<PGN>` "126992" corresponds to "SysTime" (the `<TITLE>` that'll be written), we will use the `SystemTime()` function to handle the parsing, we will log the data to the SD card, but we won't upload any information to Influx.

For EngineDynamicParamters (`PGN=127489`), we'll not only log the data to the SD card, but we will upload the first 3 Key Value pairs to InfluxDB.

## Accelerometer

We treat the accelerometer as another data source on the CAN network, from a storage and uploading perspective. There are seperate functions that handle the accelerometer parsing (as it isn't registered by the NMEA2000 library, obviously).

As an example, data from the accelerometer is stored as:

```
1650358312,133512,Accel, x=0.02, y=-31.40, z=3.12, x_g=0.00, y_g=-0.12, z_g=21.32
```

Where 133512 is an arbitrary (future proof) dummy PGN for the accelerometer, and the x,y,z accelerations values and x,y,z gyroscopic accelerations are stored to 2 d.p.

# Data Transmission System

_How do we upload the data to InfluxDB? How do we decide what to upload?_

**We use the above line format** as it is very easy to parse into [InfluxDB line protocol](https://docs.influxdata.com/influxdb/cloud/reference/syntax/line-protocol/#:~:text=InfluxDB%20uses%20line%20protocol%20to,timestamp%20of%20a%20data%20point.&text=Lines%20separated%20by%20the%20newline,Line%20protocol%20is%20whitespace%20sensitive.), for example, for the Course Over Ground and Speed Over Ground (COG/SOG) PGNs, for the boat "Jennifer":

```
COGSOG,sensor_id="Jennifer" COG=358.13,SOG=0.00 1650355763
```

## Cellular (Internet) functionallity

- We leverage the [Sim 7070G](https://www.simcom.com/product/SIM7070G.html) module to upload data via LTE-m, Nb-IoT and/or GPRS (depending on local availability).
- This works nicely with the [TinyGSM](https://github.com/vshymanskyy/TinyGSM) and [ArduinoInfluxHttpClient](https://github.com/arduino-libraries/ArduinoInfluxHttpClient) libraries, which we use to upload data to Influx via the HTTP POST API.

## InfluxDB (HTTP Post instead of file uploads)

- **We use a custom SparkInflux** library instead of the recommended Influx library because the standard Influx library only works with WiFi (and I have tried to change this).
- **We use HTTP Post instead of file uploads** for a number of reasons:
  - File uploading doesn't work well (at all) with intermittent uploads and caching
  - The only file upload formats are CSV (which is basically line uploads anyway) and .gzip (which Esp32 can't do)
  - File uploading streams from the SD card, which means that the SD card will be constantly open during the file upload period (which means we can't do any file checks) during this time, which is bad for integrity, caching, and performance.

## Caching

- We leverage a caching methodology to log successful (or otherwise) uploads because we can lose out on cellular signal, because uploading is currently blocking, and because we could lose power mid-upload/Sniff.
- Each SD card has all of the files to upload (`Jennifer_1.txt`, `Jennifer_2.txt`, etc), and also has an `upload_history.txt` file that logs the upload history.
- File upload histories can be one of the following values:

```cpp
enum UPLOAD_RESULT_ENUM
{
    NO_ATTEMPT_MADE,     // starting value
    COMPLETE_UPLOAD,     // completed upload, no need to upload again
    PARTIAL_UPLOAD,      // we've only uploaded some of this file, try uploading more
    FAILED_UPLOAD,       // for some reason, the upload failed, try again later
    REFUSED_UPLOAD,      // Influx refused the file. There's likely a problem, never upload this.
    FAILED_TO_FIND_FILE, // Couldn't find the SD card on the file, so never upload this.
};
```

File upload histories are stored in the `upload_history.txt` with the following format:

```
<DATAFILE NAME>|UP_RSLT=A|CR_DATE=B|UP_DATE=C|UP_ATMP=D|LN_UPLD=E|LN_CHCK=F|SK_CHAR=G                                                                                             \n
```

We add trailing white space " " so that each line is 180 characters long, terminating with a new line character. This means that we can edit specific lines and values (like UPLOAD_RESULTs) much faster when needed. For example, the following line (ignoring trailing whitespace and new line) still has 60 character spaces available:

```
/Jennifer_167.txt|UP_RSLT=2|CR_DATE=1651802965|UP_DATE=1651808041|UP_ATMP=249|LN_UPLD=1786|LN_CHCK=21705|SK_CHAR=2130374
```

This means that `Jennifer_167.txt` has partially uploaded a total of `1786` points(/lines). It's taken us 249 times (around 7 data points per upload, staying real-time) to upload to this point (with at least one of your uploads being succesfully, obviously), and we last attempted any upload on `Friday, May 6, 2022 3:34:01 AM` (GMT), with the session being created on `Friday, May 6, 2022 2:09:25 AM`.

We've checked a total of `21,705` lines (PGN points), and if we want to pick up where we left off we need to seek to character `2130374`.
