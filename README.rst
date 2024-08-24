Tempus Redux |Build Status|
===========================

ESP32 "Time from NPL" (MSF) Radio clock signal generator.

Requires an ESP32-S3.

    "You give that here!" shouted Mrs Ogg, as Susan held it out of her reach.
    She could feel the power in the thing. It seemed to pulse in her hand.

    "Have you any idea what this *is*, Mrs Ogg?" she said, opening her hand to
    reveal the little glass bulbs.

    "Yes, it's an egg-timer that don't work!" Mrs Ogg sat down hard in her
    overstuffed chair, so that her little legs rose off the floor for a moment.

    "It looks to be like a day, Mrs Ogg. A day's worth of time."
    Mrs Ogg glanced at Susan, and then at the little hourglass in her hand.
    ...
    Susan read the words etched on the metal base of the lifetimer: *Tempus
    Redux*. "Time Returned", she said.

    -- `Terry Pratchett <https://en.wikipedia.org/wiki/Terry_Pratchett>`_
    (`Thief of Time, 2001 <https://en.wikipedia.org/wiki/Thief_of_Time>`_)


Usage
-----

Connect to a WiFi network that has one or more NTP servers advertised by the
DHCP server and the `"Time from NPL" (MSF) <https://en.wikipedia.org/wiki/Time_from_NPL_(MSF)>`_
radio time signal will be output on GPIO1 when the time is synced from the
network.

LED Status
~~~~~~~~~~

.. list-table::
   :widths: 30 40 30
   :header-rows: 1

   * - Colour
     - Time in sync
     - Transmitting
   * - Green
     - Yes
     - Yes
   * - Blue
     - No (2+ minutes)
     - Yes
   * - Yellow
     - No (3+ hours)
     - No
   * - Red
     - Yes
     - No

Build
-----

This project can be built with the `ESP-IDF build system
<https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html>`_.

Configure::

    idf.py set-target esp32s3
    idf.py menuconfig

Under "Component config" you'll find "Tempus Redux" where you can configure the
WiFi network and whether the output is active low or not.

Build::

    idf.py build

Flash::

    idf.py flash

.. |Build Status| image:: https://jenkins.uuid.uk/buildStatus/icon?job=tempus-redux%2Fmain
