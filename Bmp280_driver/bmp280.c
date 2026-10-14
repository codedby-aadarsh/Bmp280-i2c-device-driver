#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aadarsh Mandal");
MODULE_DESCRIPTION("A simple bmp280 i2c device driver");
MODULE_VERSION("0.1");

typedef s32 BMP280_S32_t;
typedef u32 BMP280_U32_t;
typedef s64 BMP280_S64_t;

// Force the compiler to pack the struct to exactly 22 bytes
#pragma pack(push, 1)
typedef union{
    u8 calib_buf[24];
    struct{
        u16 dig_T1;
        s16 dig_T2;
        s16 dig_T3;
        u16 dig_P1;
        s16 dig_P2;
        s16 dig_P3;
        s16 dig_P4;
        s16 dig_P5;
        s16 dig_P6;
        s16 dig_P7;
        s16 dig_P8;
        s16 dig_P9;
    }__attribute__((packed)) fields; ;
}Bmp_280_calib;
#pragma pack(pop)

Bmp_280_calib dev_calib;
u8 buffer[6]; // buffer for reading the 6 byte of pressure and temp raw value
BMP280_S32_t t_fine;
BMP280_U32_t bmp280_compensate_P_int64(BMP280_S32_t adc_P);

/* convert the raw temperature data to a temp value */
BMP280_S32_t bmp280_compensate_T_int32(BMP280_S32_t adc_T) {
    BMP280_S32_t var1, var2, T;

    // Fixed en-dashes (–) to standard ASCII hyphens (-)
    var1 = ((((adc_T >> 3) - ((BMP280_S32_t)dev_calib.fields.dig_T1 << 1))) * ((BMP280_S32_t)dev_calib.fields.dig_T2)) >> 11;
    
    var2 = (((((adc_T >> 4) - ((BMP280_S32_t)dev_calib.fields.dig_T1)) * ((adc_T >> 4) - ((BMP280_S32_t)dev_calib.fields.dig_T1))) >> 12) * 
            ((BMP280_S32_t)dev_calib.fields.dig_T3)) >> 14;
    
    t_fine = var1 + var2;
    
    T = (t_fine * 5 + 128) >> 8;
    return T;
}

/* convert the raw data to the pressure */
BMP280_U32_t bmp280_compensate_P_int64(BMP280_S32_t adc_P) {
    BMP280_S64_t var1, var2, p;

    var1 = ((BMP280_S64_t)t_fine) - 128000;
    var2 = var1 * var1 * (BMP280_S64_t)dev_calib.fields.dig_P6;
    var2 = var2 + ((var1 * (BMP280_S64_t)dev_calib.fields.dig_P5) << 17);
    var2 = var2 + (((BMP280_S64_t)dev_calib.fields.dig_P4) << 35);
    var1 = ((var1 * var1 * (BMP280_S64_t)dev_calib.fields.dig_P3) >> 8) + ((var1 * (BMP280_S64_t)dev_calib.fields.dig_P2) << 12);
    var1 = (((((BMP280_S64_t)1) << 47) + var1)) * ((BMP280_S64_t)dev_calib.fields.dig_P1) >> 33;

    if (var1 == 0) {
        return 0; // Avoid exception caused by division by zero
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((BMP280_S64_t)dev_calib.fields.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((BMP280_S64_t)dev_calib.fields.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((BMP280_S64_t)dev_calib.fields.dig_P7) << 4);

    return (BMP280_U32_t)p;
}

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

    /**
     * Now we got to read the block of the 6 bytes , at once 
     * rather than reading the individual register its necessary 
     * to read all at once to take the right value all at once 
     * where the pressure and temp register forms a 6 byte continous
     * register starting from the 0xF7 to 0xFC.
     */
    ret = i2c_smbus_read_i2c_block_data(client,0xF7,6,buffer);
    if (ret < 0)
    {
        pr_err("bmp280_driver: Failed to read from I2C bus! Error code: %d\n", ret);
        return ret;
    }
    pr_info("bmp280_driver:Press raw data read: %d\n",((u32)buffer[0]<<12)|((u32)buffer[1]<<4)|((u32)buffer[2]>>4));
    pr_info("bmp280_driver:Temp raw data read: %d\n",((u32)buffer[3]<<12)|((u32)buffer[4]<<4)|((u32)buffer[5]>>4)); 
    ret = i2c_smbus_read_i2c_block_data(client,0x88,sizeof(Bmp_280_calib),dev_calib.calib_buf);
    if (ret < 0)
    {
        pr_err("bmp280_driver: Failed to read from I2C bus! Error code: %d\n", ret);
        return ret;
    }
    BMP280_S32_t press  = ((u32)buffer[0]<<12)|((u32)buffer[1]<<4)|((u32)buffer[2]>>4);
    BMP280_S32_t temp = ((u32)buffer[3]<<12)|((u32)buffer[4]<<4)|((u32)buffer[5]>>4);

    pr_info("bmp280_driver:curr_temp : %d curr_press : %d\n",bmp280_compensate_T_int32(temp),bmp280_compensate_P_int64(press)); 

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
