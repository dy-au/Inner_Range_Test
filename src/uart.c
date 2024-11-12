/*******************************************************************************
 * @file main.c
 * @author Dawei Yang
 * @date 11/11/2024
 * @brief Inner Range Embedded Programming Test uart file
 ******************************************************************************/
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "uart.h"

/*******************************************************************************
 * Macros
 ******************************************************************************/
#define NULL ((void *)0)

#define UART_CONTROL_STATUS_REG         0x80000120
#define UART_DATA_REG                   0x80000122

#define RX_NOT_EMPTY                    (1 << 0)
#define TX_NOT_FULL                     (1 << 1)
#define RX_ERROR                        (1 << 2)
#define TX_ENABLE                       (1 << 13)
#define RX_ENABLE                       (1 << 14)
#define INT_ENABLE                      (1 << 15)

#define UART_INTERRUPT_MASK             0x0000FFFFU
#define INTERRUPT_DISABLE()             (*(volatile uint16_t*)UART_CONTROL_STATUS_REG &= ~(INT_ENABLE & UART_INTERRUPT_MASK))
#define INTERRUPT_ENABLE()              (*(volatile uint16_t*)UART_CONTROL_STATUS_REG |= (INT_ENABLE & UART_INTERRUPT_MASK))
#define IS_INTERRUPT_ENABLED()          (*(volatile uint16_t*)UART_CONTROL_STATUS_REG &= (INT_ENABLE & UART_INTERRUPT_MASK))

/*******************************************************************************
 * Types
 ******************************************************************************/
typedef struct mutex_s
{ 
    volatile uint32_t locked; 
} mutex_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/
queue_t rx_queue, tx_queue;

static bool m_is_initialised = false;
static volatile uint32_t m_received_bytes_count = 0;
static mutex_t uart_mutex; 

/*******************************************************************************
 * Static Function Prototypes
 ******************************************************************************/
static void mutex_init(mutex_t *mutex) 
{ 
    mutex->locked = 0; 
} 

static void mutex_lock(mutex_t *mutex) 
{ 
    while (__atomic_test_and_set(&mutex->locked, __ATOMIC_ACQUIRE)) ;
} 

static void mutex_unlock(mutex_t *mutex) 
{ 
    __atomic_clear(&mutex->locked, __ATOMIC_RELEASE); 
}

static uart_status_t queue_push(queue_t * const queue, uint8_t byte) 
{
    uart_status_t ret = QUEUE_BUFFER_NO_ERROR;
    uint32_t interrupt_state = IS_INTERRUPT_ENABLED();

    INTERRUPT_DISABLE();

    if (queue->count < QUEUE_SIZE) 
    {
        if(queue->tail >= QUEUE_SIZE)
        {
            queue->tail = 0;
        }
        
        queue->data[queue->tail] = byte;
        queue->tail++;
        queue->count++;
    }
    else
    {
        ret = QUEUE_BUFFER_OVERFLOW;
    }

    if (interrupt_state)
    {
        INTERRUPT_ENABLE();
    }

    return ret;
}

static uart_status_t queue_pop(queue_t * const queue, uint8_t *byte) 
{
    uart_status_t ret = QUEUE_BUFFER_NO_ERROR;
    uint32_t interrupt_state = IS_INTERRUPT_ENABLED();

    INTERRUPT_DISABLE();

    if(byte != NULL)
    {
        if (queue->count > 0)
        {
            *byte = queue->data[queue->head];
            if(++queue->head >= QUEUE_SIZE)
            {
                queue->head = 0;
            }
            queue->count--;
        }
        else
        {
            ret = QUEUE_BUFFER_EMPTY;
        }
    }
    else
    {
        ret = INPUT_ERROR;
    }
    
    if (interrupt_state)
    {
        INTERRUPT_ENABLE();
    }

    return ret;
}

void __attribute__((interrupt("IRQ"))) uart_interrupt_handler(void) 
{
    // Read status register
    uint16_t status = *(volatile uint16_t*)UART_CONTROL_STATUS_REG;

    // Receive byte
    if ((status & RX_NOT_EMPTY) && (status & RX_ENABLE))
    {
        uint8_t rx_byte = *(volatile uint8_t*)UART_DATA_REG;
        
        // Push byte to receive queue
        if (queue_push(&rx_queue, rx_byte) == QUEUE_BUFFER_NO_ERROR) 
        {
            // Increment received bytes counter
            m_received_bytes_count++;
        } 
    }

    // Transmit byte
    if ((status & TX_NOT_FULL) && (status & TX_ENABLE))
    {
        uint8_t tx_byte;

        // Pop byte from transmit queue
        if (queue_pop(&tx_queue, &tx_byte) == QUEUE_BUFFER_NO_ERROR) 
        {
            *(volatile uint8_t*)UART_DATA_REG = tx_byte;
        } 

    }
}

void uart_init(void)
{
    if(false == m_is_initialised)
    {
        // Initialize queues
        memset(&tx_queue, 0, sizeof(tx_queue));
        memset(&rx_queue, 0, sizeof(rx_queue));

        // Initialise uart mutex
        mutex_init(&uart_mutex);


        // Clear received bytes counter
        m_received_bytes_count = 0;

        // TODO: Place platform specific code to set up the UART configuration and its interrupt handler

        // Enable uart interrupts
        *(volatile uint16_t*)UART_CONTROL_STATUS_REG |= (RX_ENABLE | TX_ENABLE | INT_ENABLE);

        m_is_initialised = true;
    }
}

uart_status_t uart_write_bytes(queue_t *queue, const uint8_t* data, uint32_t length) 
{
    uart_status_t ret = QUEUE_BUFFER_NO_ERROR;
    uint32_t interrupt_state = IS_INTERRUPT_ENABLED();
    
    INTERRUPT_DISABLE();
    
    if((data != NULL) && ((length + queue->count) <= QUEUE_SIZE))
    {
        mutex_lock(&uart_mutex);

        for (uint32_t i = 0; i < length && ret; i++) 
        {
            ret = queue_push(queue, data[i]);
        }

        mutex_unlock(&uart_mutex);
    }
    else
    {
        ret = INPUT_ERROR;
    }
    
    if (interrupt_state)
    {
        INTERRUPT_ENABLE();
    }

    return ret;
}

uart_status_t uart_read_blocking(uint8_t * const output_buffer, uint32_t length) 
{
    uint32_t bytes_read = 0;
    uint32_t interrupt_state = IS_INTERRUPT_ENABLED();
    uart_status_t ret = QUEUE_BUFFER_NO_ERROR;
    
    if(output_buffer != NULL)
    {
        while (bytes_read < length) 
        {
            mutex_lock(&uart_mutex);

            INTERRUPT_DISABLE();

            if (ret = queue_pop(&rx_queue, &output_buffer[bytes_read]) == QUEUE_BUFFER_NO_ERROR)
            {
                bytes_read++;
            }

            if (interrupt_state)
            {
                INTERRUPT_ENABLE();
            }

            mutex_unlock(&uart_mutex);

            if (ret == QUEUE_BUFFER_EMPTY)
            {
                // TODO: Place platform specific sleep function here
            }
        }
    }
    else
    {
        ret = INPUT_ERROR;
    }

    return ret;
}

uart_status_t uart_read_nonblocking(uint8_t* output_buffer, uint32_t *max_length) 
{
    uint32_t bytes_read = 0;
    uint32_t interrupt_state = IS_INTERRUPT_ENABLED();
    
    if(output_buffer == NULL)
    {
        return INPUT_ERROR;
    }

    mutex_lock(&uart_mutex);

    INTERRUPT_DISABLE();
    
    while (bytes_read < *max_length)  
    {
        if(queue_pop(&rx_queue, &output_buffer[bytes_read]) == QUEUE_BUFFER_NO_ERROR)
        {
            bytes_read++;
        }
        else
        {
            // rx queue empty
            break;
        }
    }
    
    if (interrupt_state)
    {
        INTERRUPT_ENABLE();
    }

    *max_length = bytes_read;

    mutex_unlock(&uart_mutex);

    return QUEUE_BUFFER_NO_ERROR;
}

uint32_t uart_reveived_bytes(void)
{
    return m_received_bytes_count;
}

uint32_t uart_bytes_in_receive_queue(void)
{
    return rx_queue.count;
}

uint32_t uart_bytes_in_transmit_queue(void)
{
    return tx_queue.count;
}

bool uart_test(void) 
{
    uint8_t tx_data[] = "Hello, UART!";
    uint8_t rx_data[QUEUE_SIZE];
    uint32_t bytes_read;
    
    // Test write receive queue
    if(uart_write_bytes(&rx_queue, tx_data, sizeof(tx_data)) != QUEUE_BUFFER_NO_ERROR)
    {
        return false;
    }
    
    // Test non-blocking read
    bytes_read = 1000;
    if(uart_read_nonblocking(rx_data, &bytes_read) != QUEUE_BUFFER_NO_ERROR)
    {
        return false;
    }

    if(sizeof(tx_data) != bytes_read)
    {
        return false;
    }
    
    // Test write receive queue
    if(uart_write_bytes(&rx_queue, tx_data, sizeof(tx_data)) != QUEUE_BUFFER_NO_ERROR)
    {
        return false;
    }

    // Test blocking read
    if(uart_read_blocking(rx_data, sizeof(tx_data)) != QUEUE_BUFFER_NO_ERROR)
    {
        return false;
    }

    // Test total number of bytes received
    if((sizeof(tx_data) * 2) != uart_reveived_bytes())
    {
        return false;
    }
    
    if(uart_write_bytes(&rx_queue, tx_data, 1000) != INPUT_ERROR)
    {
        return false;
    }

    // Test write transmit queue
    if(uart_write_bytes(&tx_queue, tx_data, sizeof(tx_data)) != QUEUE_BUFFER_NO_ERROR)
    {
        return false;
    }

    // Test read how many bytes are in the transmit queue
    if(uart_bytes_in_transmit_queue() != sizeof(tx_data))
    {
        return false;
    }

    return true;
}