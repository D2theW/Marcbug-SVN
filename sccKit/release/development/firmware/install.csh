#!/usr/bin/tcsh -f

# Make sure that this is executed by root...
if ( "`whoami`" != "root" ) then
  echo "You need to be root to install the firmware..."
  exit
endif

# Find bin folder (manually: export binpath=/opt/sccKit/<version>/bin)...
setenv sccKitPath ../

# Install the new driver
echo "Sending reset to FPGA & SCC..."
# In case we are CRB this will work:
$sccKitPath/bin/sccBmc -c "board_reset" > /dev/null
# In case we are RLB this will work:
$sccKitPath/bin/sccBmc -c "reset scc" > /dev/null
$sccKitPath/bin/sccBmc -c "reset fpga" > /dev/null
if (`lsmod | grep crbif` != "") then
  echo "Unloading driver..."
  rmmod crbif || exit
else
  echo "No driver loaded..."
endif

if (`which dkms >& /dev/null && echo "dkms available"` != "") then
  echo "Installing driver package using dkms..."
  if ( -d /lib/modules/`uname -r`/kernel/drivers/mcedev ) then
    rm -rf /lib/modules/`uname -r`/kernel/drivers/mcedev >& /dev/null
    depmod -a
  endif
  dpkg -r crbif-dkms
  dpkg -i PCIeDriver/crbif-dkms_1.1.3.deb
else
  echo "Error: dkms is not available. Please install it and try again! Aborting..."
  exit
endif
echo "Driver is installed and will be loaded during reboot..."
echo ""

# Install the BMC firmware inclunding bitstream updates
setenv bmcServer `grep -i crbserver ../../systemSettings.ini | sed "s/.*=//" | sed "s/:.*//"`

echo "Performing 'power off' on BMC ($bmcServer)..."
$sccKitPath/bin/sccBmc -c "power off" > /dev/null

if (`grep -i RockyLake ../../systemSettings.ini` != "") then
  echo "Installing Rockylake BMC update (1.11) and CPLD update (1.09) including bitstream update..."
  echo "Be prepared to enter BMC root password (rlbbmc)..."
  scp -r RockyLake/* root@${bmcServer}:/media/usb/
  $sccKitPath/bin/sccBmc -c "reset bmc" > /dev/null
  echo "Restarting BMC (will be offline for about 60 seconds) to apply update..."
  echo "Installed default bitstream is: /mnt/flash4/rl_20110627_abcd.bit"
  echo 'Please change sccMacEnable in systemSettings.ini (a and/or b and/or c and/or d). e.g. "sccMacEnable=abc"'
endif

if (`grep -i RockyLake ../../systemSettings.ini` == "") then
  echo "Installing Copperridge BMC update (1.07) including bitstream update..."
  echo "Be prepared to enter BMC root password (rckbmc) several times..."
  $sccKitPath/bin/sccBmc -c "terminate" > /dev/null
  scp root@${bmcServer}:/flash/default.bit Copperridge/old.bit
  ls -al Copperridge/old.bit | grep 6891823 > /dev/null && scp Copperridge/cr_220_20100527.bit root@${bmcServer}:/flash/default.bit 
  ls -al Copperridge/old.bit | grep 5380399 > /dev/null && scp Copperridge/cr_155_20100526.bit root@${bmcServer}:/flash/default.bit 
  rm -rf Copperridge/old.bit
  scp Copperridge/autorun root@${bmcServer}:/flash/
  ssh root@${bmcServer} "/flash/power_off > /dev/null; echo Switching power off...; sleep 3; reboot"
  echo "Restarting BMC (will be offline for about 60 seconds) to apply update..."
endif

echo ""
echo "Update completed... Please perform the following steps:"
echo '=> Allow BMC 60 seconds to boot (Command "../bin/sccBmc -c set" needs to return correct bitstream)'
echo '=> Perform "power on" with command "../bin/sccPowercycle -s"'
echo "=> Train System interface (either with nuke button in GUI"
echo "   or with 'sccBmc -i')..."
