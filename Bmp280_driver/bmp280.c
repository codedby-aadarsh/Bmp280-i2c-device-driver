#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aadarsh Mandal");
MODULE_DESCRIPTION("A simple bmp280 i2c device driver");
MODULE_VERSION("0.1");

u8 lsb,msb,xlsb;

static int my_bmp280_probe(struct i2c_client *client)
{
    int ret;
    pr_info("Using bmp280 driver probing\n");

    // read the chip id
    ret = i2c_smbus_read_byte_data(client, 0xD0);

    // negative value indicates the failure or disconnected bus
    if (ret < 0)
    {
        pr_err("bmp280_driver: Failed to read from I2C bus! Error code: %d\n", ret);
        return ret;
    }
    // compare the return value agains the expected id
    if (ret != 0x58)
    {
        pr_info("bmp2780_driver: Found, but the chip id is 0x%2x is invalid\n", ret);
        return -ENODEV;
    }
    pr_info("bmp280_driver: Sucess! Found chip id is 0x%2x is valid\n", ret);
    ret = i2c_smbus_write_byte_data(client, 0xE0, 0xB6);
    usleep_range(2000, 3000);
    if (ret < 0)
    {
        pr_err("bmp280_driver: Failed to reset from I2C bus! Error code: %d\n", ret);
        return ret;
    }
    pr_info("bmp280_driver: Reset status sucessfull");
    /**
     * configuring the control register to choose
     * the indoor navigation mode
     * 1. mode : Normal mode
     * 2. temp oversampling : 2
     * 3. pressure oversampling: 16
     * so the hex value for this register we have to enter will
     * be 0x57.
     * */
    ret = i2c_smbus_write_byte_data(client, 0xF4, 0x57);
    if (ret < 0)
    {
        pr_info("bmp280_driver: Failed to configure from I2C bus !Error code: %d\n", ret);
        return ret;
    }
    pr_info("bmp280_driver: Configured the control register sucessfully\n");

    /**
     * Selecting the standby period = 0.5ms for the normal mode, and also setting
     * the IIR filter coefficinet value=16,
     */
    ret = i2c_smbus_write_byte_data(client, 0xF5, 0x10);
    if (ret < 0)
    {
        pr_info("bmp280_driver: Failed to configure from I2C bus !Error code: %d\n", ret);
        return ret;
    }
    pr_info("bmp280_driver: Configured the standby time and iir coefficient sucessfully\n");
    pr_info("bmp280_driver: Performing the last check up before commencing the initialisation successful, by reading the configure register values\n"); 
    ret = i2c_smbus_read_byte_data(client,0xF4);
    if (ret < 0)
    {
        pr_err("bmp280_driver: Failed to read from I2C bus! Error code: %d\n", ret);
        return ret;
    }
    if(ret!=0x57){
        pr_err("bmp280_driver: control register value 0x%02x is invalid", ret);
        return -EINVAL;
    }
    pr_info("bmp280_driver:Control register read value 0x%02x is valid\n",ret); 
    
    ret = i2c_smbus_read_byte_data(client,0xF5);
    if (ret < 0)
    {
        pr_err("bmp280_driver: Failed to read from I2C bus! Error code: %d\n", ret);
        return ret;
    }
    if(ret!=0x10){
        pr_err("bmp280_driver:Config register value 0x%02x is invalid", ret);
        return -EINVAL;
    }
    pr_info("bmp280_driver:Config register read value 0x%02x is valid\n",ret); 
    pr_info("bmp280_driver:Driver has been initialised sucessfully\n"); 

    msb = i2c_smbus_read_byte_data(client,0xFA);
    if (msb < 0)
    {
        pr_err("bmp280_driver: Failed to read from I2C bus! Error code: %d\n", msb);
        return msb;
    }
    lsb = i2c_smbus_read_byte_data(client,0xFB);
    if (lsb < 0)
    {
        pr_err("bmp280_driver: Failed to read from I2C bus! Error code: %d\n", lsb);
        return lsb;
    }
    xlsb = i2c_smbus_read_byte_data(client,0xFC);
    if (xlsb < 0)
    {
        pr_err("bmp280_driver: Failed to read from I2C bus! Error code: %d\n", xlsb);
        return xlsb;
    }
    u32 temp = ((u32)msb<<12)|((u32)lsb<<4)|((u32)xlsb>>4);
    pr_info("bmp280_driver:Temp raw data read: %d\n",temp); 
    return 0;
}
static void my_bmp280_remove(struct i2c_client *client)
{
    pr_info("Calling the bmp280 driver remove sucessful\n");
}
/**
 * The device tree match table,
 * In this table you write the comptabile string which must be match
 * against the string written in the .dts file.
 */
static const struct of_device_id bmp280_of_match[] = {
    {.compatible = "custom,my-bmp280"},
    {}};

/**
 * This part here helps with the auto loading of the driver, when the linux
 * finds hardware which matches the string with the compatible you pass in
 * the id table it knows to loads the driver for that device.
 */
MODULE_DEVICE_TABLE(of, bmp280_of_match);

static const struct i2c_device_id bmp280_i2c_id[] = {
    {"my-bmp280", 0},
    {}};
MODULE_DEVICE_TABLE(i2c, bmp280_i2c_id);

static struct i2c_driver my_bmp280_driver = {
    .driver = {
        .name = "bmp280_driver",
        .of_match_table = bmp280_of_match},
    .probe = my_bmp280_probe,
    .remove = my_bmp280_remove,
    .id_table = bmp280_i2c_id,
};

module_i2c_driver(my_bmp280_driver);
