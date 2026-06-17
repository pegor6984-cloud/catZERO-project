# catZERO-project
```text
 ██████╗ █████╗ ████████╗███████╗███████╗██████╗  ██████╗
██╔════╝██╔══██╗╚══██╔══╝╚══███╔╝██╔════╝██╔══██╗██╔═══██╗
██║     ███████║   ██║     ███╔╝ █████╗  ██████╔╝██║   ██║
██║     ██╔══██║   ██║    ███╔╝  ██╔══╝  ██╔══██╗██║   ██║
╚██████╗██║  ██║   ██║   ███████╗███████╗██║  ██║╚██████╔╝
 ╚═════╝╚═╝  ╚═╝   ╚═╝   ╚══════╝╚══════╝╚═╝  ╚═╝ ╚═════╝
```
Hello my name is Egor I'm 13 I live in Germany I created my new catZERO project on ESP32-C3.Based on this project, ESP32-C3 Super Mini is used,nrf24,Ir capture/transmit,oled display (128x64),3 clock buttons, 2.2 kOhm resistor,3.3 kOhm resistor (2x),6.8 kOhm resistor charging board, boost board from 3.7 to 5 Volts, 650 mah battery, breadboards (2x), wires.I made some changes to the program and now there is an SD card lift.And now a control console has appeared there, but it is not quite completed.Due to a lack of pins for the SD card, we had to use a new technique for connecting clock buttons to the board.Now all the buttons are connected via resistors to one pin and everything works fine
Now a Wi-Fi chat has been added. If you are connected to Wi-Fi and you go to the IP address that the program gave you, then you can chat from device to device. Then a battery calculation was added. If you use exactly the same battery calculation as mine. To be more precise, 650 mah And if you have a different battery calculation, then you can set this in the program I have done such a great job so please can any of you promote me Был улучшен nrf Jammer но он всё равно плохо работает и был лучшим TV-B-Gone теперь он реально может включать и выключать телевизоры
Sorry for the fact that I was gone for so long the project has undergone very big changes now We have switched to a new processor Sorry that it is more expensive but now there are more functions We have switched to the ESP32 S3 Super Mini raft a new module has been added that's all CC1101 SD card module circuit diagrams I will now post and now a new command will appear firstly for CC1101 and working Bad USB scripts have appeared
And now there are two new RGB LEDs, one we place under the keyboard, our navigation buttons, and the second I use as a piece of LED strip for eight RGB LEDs

This code included an Internet connection and a beautiful Internet icon when connected to it

Connectio:

rgd (Keyboard):
vcc-5v
gnd-gnd
in-16

rgb (BOARD):
vcc-5v
gnd-gnd
in-14

oled(128x64):

gnd-gnd

vcc-5v

scl-9D

sda-8D

button(reset):
reset-1D

buttons:

3.3V
 
 |

[3.3k] ← tightening
 
 |
 
 +-------> GPIO17 (for testing; can be replaced later)
 
 |
 
 +--[2.2k]----Up button----GND
 
 |
 
 +--[3.3k]--Down Button-----GND
 
 |
 
 +--[6.8k]---OK button-------GND

nrf24:

CE-11D

CSN-10D

MOSI-7D

SCK-6D

MISO-2D

Ir capture/transmit:

capture:

gnd-gnd

vcc-vcc

s-5D

transmit:

gnd-gnd

vcc-18D

mikro sd 

gnd-gnd

vcc-3.3v

cs-20D

mosi-7D

miso-2D

cls-6D

CatHACK Features:

WIFI:

Scan

spectrum

Wi-fi chat

nRF24:

Spectrum

jammer

Recheck

IR:

capture

transmit

Erase All

TV-B-Gone

console:

As I already said, it will be possible to transport different teams there
There is also a secret panel and you can change the code for it in the program

Settings:

info

Timeout

Reset

Reboot

That's all for now, now I'll sit and improve my code!!

(Guys, I was afraid of hate and that's why I renamed my project to catZero!!!)
