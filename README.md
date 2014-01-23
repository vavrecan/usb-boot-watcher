UsbBootWatcher
================
This is simple service that allows you to boot installed Windows NT (XP, 7/8, Server 2012) from USB HDD or USB stick. 
Method is based on [findings from Dietmar Stoelting](http://www.911cd.net/forums/index.php?showtopic=14181) - it allows USB drivers to be loaded during boot time (no more BSOD 0x0000007B). 

To make your windows bootable from USB device, you normally have to modify driver (.inf files) and system to accept digitally unsigned drivers. This service allows you to avoid changes to drivers and have the digital signature enforcement enabled.

This is possible because the UsbBootWatcher service monitors registry for changes under following paths:
```
SYSTEM\CurrentControlSet\Services\usbstor
SYSTEM\CurrentControlSet\Services\usbehci
SYSTEM\CurrentControlSet\Services\usbohci
SYSTEM\CurrentControlSet\Services\usbuhci
SYSTEM\CurrentControlSet\Services\usbhub
```

It sets value `Start` to `0` and value `Group` to `Boot Bus Extender` when changed by driver updates / invalidations. 

UsbBootWatcher is updating usbstor, usbehci, usbohci, usbuhci, usbhub drivers, but you can also specify custom driver by addint it to `UsbBootWatcher.conf`.

Installing Windows to USB device
----------------

Microsoft does not officially allows you to install Windows to USB device, hovewer, there is simple trick to do this - you can install it like OEM partner. To do so, you will need existing windows installation with admin permissions (or use Recovery console).

You will need these tools: imagex, bcdboot. You can get them from [here](http://www.rmprepusb.com/tutorials/getwaiktools).

Then prepare your drive (create primary partition and make it active on your usb drive) using diskpart:
```
diskpart
list disk
select disk __DISK__
list partition
select partition __PARTITION__
format fs=NTFS quick 
active 
assign 
exit
```

Now you can use imagex to copy your installation image to the device (considering d: is your USB device and e: windows installation medium containing install.wim):
```
imagex /apply e:\sources\install.wim 3 d:
bcdboot d:\Windows /s d: /v
```
[More about imagex](http://technet.microsoft.com/en-us/library/cc749447%28v=ws.10%29.aspx)
[More about bcdboot](http://technet.microsoft.com/en-us/library/dd744347%28v=ws.10%29.aspx)

Done! And now you can install UsbBootWatcher to the target device like (it will automatically copy to target device and create service there): 
```
UsbBootWatcher /prepare d:\Windows\System32
```

Boot your USB device and continue installation. If you receive 0x0000007B BSOD after first installation step (device installations), just re-run UsbBootWatcher prepare command from your working windows installation or recovery console. 

## Authors
- [Marek Vavrecan](mailto:vavrecan@gmail.com)
- [Klaus Daniel Terhorst](mailto:nightos@gmail.com)
- [Donate by PayPal](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=DX479UBWGSMUG&lc=US&item_name=Friend%20List%20Watcher&currency_code=USD&bn=PP%2dDonationsBF%3abtn_donateCC_LG%2egif%3aNonHosted)

## License
- [GNU General Public License, version 2](http://www.gnu.org/licenses/gpl-2.0.html)
