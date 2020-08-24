
#define I2C_READ    1
#define I2C_WRITE   0

/* Initialization of the I2C bus interface. Need to be called only once. */
static void i2c_init(void)
{
    i2c_wsda(1);
    i2c_wscl(1);
    i2c_delay();
}


/* Send one byte to I2C device. Returns 0=OK, 1=failed. */
static uint8_t i2c_write(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        i2c_wscl(0); i2c_delay();
        i2c_wsda(data & 0x80); i2c_delay();
        i2c_wscl(1); i2c_delay();
        data <<= 1;
    }
    i2c_wscl(0); i2c_wsda(1); i2c_delay();
    i2c_wscl(1); i2c_delay();
    uint8_t result = i2c_rsda() ? 1 : 0;
    return result;
}


/* Read one byte from the I2C device, ACK as required. */
static uint8_t i2c_read(bool ack)
{
    uint8_t data = 0;
    for (uint8_t i = 0; i < 8; i++) {
        data <<= 1;
        i2c_wscl(0); i2c_wsda(1); i2c_delay();
        i2c_wscl(1); i2c_delay();
        data |= i2c_rsda() ? 1 : 0;
    }
    i2c_wscl(0); i2c_wsda(ack ? 0 : 1); i2c_delay();
    i2c_wscl(1); i2c_delay();
    return data;
}


/* Issues a start condition and sends address. Returns 0=OK, 1=failed. */
static uint8_t i2c_start(uint8_t address)
{
    i2c_delay();
    i2c_wsda(0); i2c_delay();
    return i2c_write(address);
}


/* Issues a repeated start condition and sends address. Returns 0=OK, 1=failed. */
static uint8_t i2c_rep_start(uint8_t address)
{
    i2c_delay();
    i2c_wscl(0); i2c_delay();
    i2c_wsda(1); i2c_delay();
    i2c_wscl(1); i2c_delay();
    i2c_wsda(0); i2c_delay();
    return i2c_write(address);
}


/* Terminates the data transfer and releases the I2C bus. */
static void i2c_stop(void)
{
    i2c_delay();
    i2c_wscl(0); i2c_wsda(0); i2c_delay();
    i2c_wscl(1); i2c_delay();
    i2c_wsda(1); i2c_delay();
}


/* Issues a start condition and sends address. Use ACK polling until timeout. Returns 0=OK, 1=failed. */
static uint8_t i2c_start_wait(uint8_t address, uint32_t timeout)
{
    uint32_t tickstart = HAL_GetTick();
    bool wip;
    do {
        wip = i2c_start(address);
        if (wip) i2c_stop();
    } while (wip && (HAL_GetTick() - tickstart < timeout));
    return wip;
}

