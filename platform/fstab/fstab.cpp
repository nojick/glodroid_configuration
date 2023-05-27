// Android fstab file. cpp extension is used to pass C preprocessor
//<src>                                           <mnt_point> <type> <mnt_flags and options> <fs_mgr_flags>
// The filesystem that contains the filesystem checker binary (typically /system) cannot
// specify MF_CHECK, and must come before any filesystems that do specify MF_CHECK

// '<ALL>' will be replaced with '*' . '*' can't be used since it will be interpreted as a comment by C preprocessor

#ifdef platform_sunxi
#define __FILE_ENCRYPT__ fileencryption=adiantum,metadata_encryption=adiantum
#else
#define __FILE_ENCRYPT__ fileencryption=aes-256-xts:aes-256-cts
#endif

#ifndef platform_qcom_msm8916

system                              /system         ext4    ro,barrier=1                          wait,first_stage_mount,logical,slotselect,avb=vbmeta_system,avb_keys=/avb
system_ext                          /system_ext     ext4    ro,barrier=1                          wait,first_stage_mount,logical,slotselect,avb=vbmeta_system
product                             /product        ext4    ro,barrier=1                          wait,first_stage_mount,logical,slotselect,avb=vbmeta_system
vendor                              /vendor         ext4    ro,barrier=1                          wait,first_stage_mount,logical,slotselect,avb=vbmeta
vendor_dlkm                         /vendor_dlkm    ext4    ro,noatime,errors=panic               wait,first_stage_mount,logical,slotselect,avb=vbmeta

/dev/block/by-name/misc             /misc           emmc    defaults                              defaults

// /dev/block/by-name/boot             /boot           emmc    defaults                              defaults,slotselect,avb=boot
// /dev/block/by-name/vendor_boot      /vendor_boot    emmc    defaults                              defaults,slotselect,avb=vendor_boot

/dev/block/by-name/metadata         /metadata       ext4    noatime,nosuid,nodev,discard,sync     wait,check,formattable,first_stage_mount
/dev/block/by-name/userdata         /data           ext4    noatime,nosuid,nodev,barrier=1        wait,check,latemount,quota,formattable,__FILE_ENCRYPT__,keydirectory=/metadata/vold/metadata_encryption

// USB storage
#ifdef platform_broadcom
/devices/platform/scb/<ALL>.pcie/<ALL>/<ALL>/<ALL>/usb<ALL>    auto   auto    defaults            voldmanaged=usb:auto,encryptable=userdata
#else
/devices/platform/soc/<ALL>/usb<ALL>                  auto            auto    defaults            voldmanaged=usb:auto,encryptable=userdata
#endif
#else
/dev/block/platform/soc@0/7824900.mmc/by-name/system    /system            ext4    noatime,ro,errors=panic wait,first_stage_mount
/dev/block/platform/soc@0/7824900.mmc/by-name/cache     /vendor            ext4    noatime,ro,errors=panic wait,first_stage_mount
/dev/block/platform/soc@0/7824900.mmc/by-name/userdata  /data              ext4    noatime,nosuid,nodev,barrier=1 wait,check,latemount,quota,formattable,__FILE_ENCRYPT__

// For firmwares
// /dev/block/platform/soc@0/7824900.mmc/by-name/modem    /vendor/odm/modem    vfat    ro,shortname=lower,uid=1000,gid=1000,dmask=227,fmask=337,context=u:object_r:firmware_file:s0 wait
/dev/block/platform/soc@0/7824900.mmc/by-name/modem     /vendor/odm/modem    vfat    defaults wait
/dev/block/platform/soc@0/7824900.mmc/by-name/persist   /vendor/odm/persist  ext4    noatime,ro,noauto_da_alloc wait

/devices/platform/soc@0/7864900.mmc/mmc_host/mmc*       auto                 auto    defaults     voldmanaged=sdcard1:auto
#endif
