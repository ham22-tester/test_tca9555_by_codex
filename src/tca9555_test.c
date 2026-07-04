#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(tca9555_test, LOG_LEVEL_INF);

#define TCA_NODE DT_NODELABEL(tca9555_u1)
#define TCA_I2C I2C_DT_SPEC_GET(TCA_NODE)

/* Numpad TCA9555PWR: A2=A1=A0=GND -> 0x20
 * P02 = ROW1, P07 = COL1
 */
#define NUMPAD_ROW1_PIN 2
#define NUMPAD_COL1_PIN 7

#define TCA9555_INPUT_PORT0 0x00
#define TCA9555_INPUT_PORT1 0x01
#define TCA9555_CONFIG_PORT0 0x06
#define TCA9555_CONFIG_PORT1 0x07

static int read_tca_word(const struct i2c_dt_spec *i2c, uint8_t reg, uint16_t *value)
{
    uint8_t low;
    uint8_t high;
    int ret;

    ret = i2c_reg_read_byte_dt(i2c, reg, &low);
    if (ret < 0) {
        return ret;
    }

    ret = i2c_reg_read_byte_dt(i2c, reg + 1, &high);
    if (ret < 0) {
        return ret;
    }

    *value = ((uint16_t)high << 8) | low;
    return 0;
}

static int sample_col_to_row(const struct device *tca, int drive_value)
{
    int ret;

    ret = gpio_pin_configure(tca, NUMPAD_ROW1_PIN, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("ROW1 P%02d input config failed: %d", NUMPAD_ROW1_PIN, ret);
        return ret;
    }

    ret = gpio_pin_configure(tca, NUMPAD_COL1_PIN,
                             drive_value ? GPIO_OUTPUT_HIGH : GPIO_OUTPUT_LOW);
    if (ret < 0) {
        LOG_ERR("COL1 P%02d output config failed: %d", NUMPAD_COL1_PIN, ret);
        return ret;
    }

    k_sleep(K_MSEC(10));
    return gpio_pin_get(tca, NUMPAD_ROW1_PIN);
}

static int sample_row_to_col(const struct device *tca, int drive_value)
{
    int ret;

    ret = gpio_pin_configure(tca, NUMPAD_COL1_PIN, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("COL1 P%02d input config failed: %d", NUMPAD_COL1_PIN, ret);
        return ret;
    }

    ret = gpio_pin_configure(tca, NUMPAD_ROW1_PIN,
                             drive_value ? GPIO_OUTPUT_HIGH : GPIO_OUTPUT_LOW);
    if (ret < 0) {
        LOG_ERR("ROW1 P%02d output config failed: %d", NUMPAD_ROW1_PIN, ret);
        return ret;
    }

    k_sleep(K_MSEC(10));
    return gpio_pin_get(tca, NUMPAD_COL1_PIN);
}

static void tca9555_test_thread(void)
{
    const struct device *tca = DEVICE_DT_GET(TCA_NODE);
    const struct i2c_dt_spec i2c = TCA_I2C;
    uint16_t input_value;
    uint16_t config_value;
    int c2r_high;
    int c2r_low;
    int r2c_high;
    int r2c_low;
    int ret;

    k_sleep(K_MSEC(1000));

    if (!device_is_ready(i2c.bus)) {
        LOG_ERR("I2C bus is not ready");
        return;
    }

    ret = read_tca_word(&i2c, TCA9555_CONFIG_PORT0, &config_value);
    if (ret < 0) {
        LOG_ERR("TCA9555 did not answer at 0x%02x: %d", i2c.addr, ret);
        return;
    }

    LOG_INF("TCA9555 answered at 0x%02x, config=0x%04x", i2c.addr, config_value);

    if (!device_is_ready(tca)) {
        LOG_ERR("TCA9555 GPIO device is not ready");
        return;
    }

    LOG_INF("Numpad test started: COL1=P%02d, ROW1=P%02d", NUMPAD_COL1_PIN, NUMPAD_ROW1_PIN);
    LOG_INF("Press the test key. If COL->ROW changes, use diode-direction col2row. If ROW->COL changes, use row2col.");

    while (1) {
        c2r_high = sample_col_to_row(tca, 1);
        c2r_low = sample_col_to_row(tca, 0);
        r2c_high = sample_row_to_col(tca, 1);
        r2c_low = sample_row_to_col(tca, 0);

        ret = read_tca_word(&i2c, TCA9555_INPUT_PORT0, &input_value);
        if (ret == 0) {
            LOG_INF("COL->ROW row(COL=1/0)=%d/%d, ROW->COL col(ROW=1/0)=%d/%d, input=0x%04x",
                    c2r_high, c2r_low, r2c_high, r2c_low, input_value);
        } else {
            LOG_ERR("TCA9555 input read failed: %d", ret);
        }

        k_sleep(K_MSEC(1000));
    }
}

K_THREAD_DEFINE(tca9555_test_tid, 1024, tca9555_test_thread,
                NULL, NULL, NULL, 7, 0, 0);
