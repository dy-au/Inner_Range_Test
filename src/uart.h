#ifndef _UART_H
#define _UART_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define QUEUE_SIZE                      256

typedef struct queue_s
{
    uint8_t data[QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} queue_t;

typedef enum 
{
    QUEUE_BUFFER_NO_ERROR = 0,
    QUEUE_BUFFER_OVERFLOW,
    QUEUE_BUFFER_EMPTY,
    INPUT_ERROR,
}uart_status_t;

extern queue_t rx_queue, tx_queue;


/**
 * @brief INitialise uart
 *
 * @param none
 * @return none
 */
void uart_init(void);

/**
 * @brief  Write n bytes of data from a byte pointer to the queue.
 * 
 * @param queue pointer
 * @param data buffer pointer
 * @param number of bytes 
 * @return INPUT_ERROR, QUEUE_BUFFER_OVERFLOW or QUEUE_BUFFER_NO_ERROR
 */
uart_status_t uart_write_bytes(queue_t *queue, const uint8_t* data, uint32_t length);

/**
 * @brief  A blocking function to read n bytes of data from the queue to a byte pointer.
 *
 * @param output buffer pointer
 * @param n bytes of data  
 * @return INPUT_ERROR or QUEUE_BUFFER_NO_ERROR
 */
uart_status_t uart_read_blocking(uint8_t *output_buffer, uint32_t length);

/**
 * @brief  A non-blocking function to read up to n bytes of data from the queue to a byte pointer.
 *
 * @param output buffer pointer
 * @param pointer of the size of the buffer
 * @return INPUT_ERROR or QUEUE_BUFFER_NO_ERROR
 */
uart_status_t uart_read_nonblocking(uint8_t *output_buffer, uint32_t *max_length) ;

/**
 * @brief Read how many bytes are in the receive queue.
 *
 * @param none
 * @return how many bytes are in the receive queue.
 */
uint32_t uart_bytes_in_receive_queue(void);

/**
 * @brief Read how many bytes are in the transmit queue.
 *
 * @param none
 * @return how many bytes are in the transmit queue.
 */
uint32_t uart_bytes_in_transmit_queue(void);

/**
 * @brief Read the total number of bytes received since power up.
 *
 * @param none
 * @return the total number of bytes received since power up.
 */
uint32_t uart_reveived_bytes(void);

/**
 * @brief uart test function.
 *
 * @param none
 * @return true: success, false: failure.
 */
bool uart_test(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _UART_H*/
