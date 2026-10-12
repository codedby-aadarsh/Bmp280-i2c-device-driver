#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aadarsh Mandal");
MODULE_DESCRIPTION("A simple bmp280 i2c device driver");
MODULE_VERSION("0.1");

static int my_bmp280_probe(struct i2c_client *client){
    pr_info("Using bmp280 driver probing\n");
    return 0;
}
static void my_bmp280_remove(struct i2c_client *client){
    pr_info("Calling the bmp280 driver remove sucessful\n");
}
/**
 * The device tree match table,
 * In this table you write the comptabile string which must be match 
 * against the string written in the .dts file.  
 */
static const struct of_device_id bmp280_of_match[]={
    {.compatible = "custom,my-bmp280"},
    {}
};

/**
 * This part here helps with the auto loading of the driver, when the linux
 * finds hardware which matches the string with the compatible you pass in
 * the id table it knows to loads the driver for that device.
 */
MODULE_DEVICE_TABLE(of,bmp280_of_match);

static const struct i2c_device_id bmp280_i2c_id[] = {
    {"my-bmp280",0},
    {}
};
MODULE_DEVICE_TABLE(i2c,bmp280_i2c_id);


static struct i2c_driver my_bmp280_driver = {
    .driver = {
        .name = "bmp280_driver",
        .of_match_table= bmp280_of_match
    },
    .probe = my_bmp280_probe,
    .remove = my_bmp280_remove,
    .id_table = bmp280_i2c_id,
};

module_i2c_driver(my_bmp280_driver);
