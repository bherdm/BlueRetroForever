**This repository is an active fork of the archived BlueRetro project (https://github.com/darthcloud/BlueRetro).**


BlueRetro was created and maintained by Jacques Gagnon, whose work redefined input support for retro systems. This fork, BlueRetroForever, continues the project under the same licensing.

Contributions and testing from the community are welcome and appreciated. My primary focus remains the Retrot Game Engine, so my contributions to BlueRetroForever will mainly serve that project.

Thank you,

bherdm

## What this fork has fixed, and who we learned from

Fixes on top of upstream's final commit, each verified on hardware or in the QEMU test suite:

* **Firmware updates stream.** The config server now takes the firmware image (and memory-card
  data) as writes without response and says so in its GATT declaration, so an updater can stream
  instead of waiting a connection interval for each chunk: a 610 KB image in about 36 s from a Linux
  central. An image the adapter rejects at the end is answered with an ATT error rather than a plain
  write response.
* **Restarts are quick and clean.** Before a restart or a deep sleep the adapter disconnects every
  peer, including the phone or browser on the config interface, so the link goes in a third of a
  second instead of timing out ten seconds later. And a software restart no longer hangs for nine
  seconds: `esp_restart` leaves the second core stalled, which only the RTC watchdog used to free.
* **Version and name reads** return the whole descriptor field instead of a fixed 23 bytes.
* **The QEMU tests** no longer depend on the build's memory layout: the memory card's 128 KB is left
  in the heap under QEMU, where the card is never brought up.

### Switch 2 controllers

Upstream added Switch 2 support in v25.10-beta and was archived soon after, with the NSO GameCube
controller's "phantom stick" and reconnection problems unfixed
([darthcloud/BlueRetro#1249](https://github.com/darthcloud/BlueRetro/issues/1249)). Three community
forks worked on it, and this fork carries their fixes with their authorship:

* [Last-Colossi/BlueRetro](https://github.com/Last-Colossi/BlueRetro) — reports are held until the
  controller's calibration has loaded, an error ack is never parsed as data, an implausible stick
  centre falls back to the defaults, a user calibration counts only under its magic word, and the
  NSO GameCube triggers' idle noise sits inside a small deadzone.
* [bjerreman/BlueRetro-Switch2Fix](https://github.com/bjerreman/BlueRetro-Switch2Fix) — up to four
  Switch 2 controllers at once: one ACL reassembly buffer per device, inbound BLE reconnection into
  a proper device slot, LTK-request handling, and re-advertising when a slot frees up.
* [RyanCopley/BlueRetro](https://github.com/RyanCopley/BlueRetro) — a SPI-flash read's answer must
  echo the address and length just requested, so a stray or duplicated answer can't shift the
  bring-up and store the wrong bytes as a key; a link that drops before its bring-up completes has
  its stored key purged; and a heap overflow in the debug trace writer is fixed. His Melee-safe
  combo defaults for GameCube builds are not taken: the shipped defaults stay upstream's, and the
  remap belongs in the configurator.

On top of those, a failed SPI read is retried the same way a stray answer is, and the QEMU test
suite gained a Switch 2 NSO GameCube controller test that walks the whole bring-up.



# BlueRetro

<p align="center"><img src="/static/PNGs/BRE_Logo_Color_Outline.png" width="600"/></p>
<br>
<p align="justify">BlueRetro is a multiplayer Bluetooth controllers adapter for various retro game consoles & computers. Lost or broken controllers? Reproduction too expensive? Need those rare and obscure accessories? Just use the Bluetooth devices you already got! The project is open source hardware & software under the CERN-OHL-P-2.0 & Apache-2.0 licenses respectively. It's built for the popular ESP32 chip. Wii, Switch, PS3, PS4, PS5, Xbox One, Xbox Series X|S & generic HID Bluetooth (BR/EDR & LE) devices are supported. Parallel 1P (Computers, NeoGeo, Supergun, JAMMA, Handheld, etc), Parallel 2P (Atari 2600/7800, Master System, Computers, etc), NES, PCE / TG16, Mega Drive / Genesis, SNES, CD-i, 3DO, Jaguar, Saturn, PSX, PC-FX, JVS (Arcade), Virtual Boy, N64, Dreamcast, PS2, GameCube & Wii extension are supported with simultaneous 4+ players using a single adapter.</p>

## READ THIS FIRST
* [Project documentation](https://github.com/darthcloud/BlueRetro/wiki)

## Need help?
* [Open a GitHub discussion](https://github.com/darthcloud/BlueRetro/discussions)

## Makers sponsoring BlueRetro
Buying BlueRetro adapters from these makers helps support the continued development of the BlueRetro firmware!\
Thanks to all sponsors!

* [Laser Bear Industries](https://www.laserbear.net)
* [Humble Bazooka](https://www.humblebazooka.com)
* [RetroOnyx](https://www.retroonyx.com/)
* [RetroTime](https://8bitmods.com/retrotime)

## Community Contribution
* BlueRetro PS1/2 Receiver by [mi213](https://twitter.com/mi213ger): 3D printed case & PCB for building DIY PS1/2 dongle.\
  https://github.com/Micha213/BlueRetro-PS1-2-Receiver
* N64 BlueRetro Mount by [reventlow64](https://twitter.com/reventlow): 3d printed mount for ESP32-DevkitC for N64.\
  https://www.prusaprinters.org/prints/90275-nintendo-64-blueretro-bluetooth-receiver-mount
* BlueRetro Adapter Case by [Sigismond0](https://twitter.com/Sigismond0): 3d printed case for ESP32-DevkitC.\
  https://www.prusaprinters.org/prints/116729-blueretro-bluetooth-controller-adapter-case
* BlueRetro AIO by [pmgducati](https://github.com/pmgducati): BlueRetro Through-hole base and cable PCBs.\
  https://github.com/pmgducati/Blue-Retro-AIO-Units
* BlueRetro HW2 internal guides by [Nostalgic Indulgences](https://twitter.com/nosIndulgences): Internal install guides\
  https://github.com/nostalgic-indulgences/BlueRetro_Internal_Installation
* BlueRetro latency test by [GamingNJncos](https://twitter.com/GamingNJncos): Documentation on how to run BlueRetro latency test\
  https://github.com/GamingNJncos/BLE-3D-Saturn-Public/tree/main/BlueRetro_Latency_Testing
* BR4N64 by [TharathielCB](https://github.com/TharathielCB): Internal BlueRetro Flex-PCB for Nintendo 64\
  https://github.com/TharathielCB/BR4N64
* BlueMemCard by [ChrispyNugget](https://github.com/ChrispyNugget): Replacement PCB for PSX memory card that incorporates PicoMemcard and BlueRetro support\
  https://github.com/ChrispyNugget/BlueMemCard
* BlueRetro HW2 QSB for GameCube by [Arthrimus](https://github.com/Arthrimus): Internal Blueretro PCB with CurrentTrigger for GameCube\
  https://github.com/Arthrimus/BlueRetro-HW2-GameCube

<br><p align="center"><img src="https://cdn.hackaday.io/images/4560691598833898038.png" height="200"/></p>

