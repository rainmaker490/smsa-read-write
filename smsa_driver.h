#ifndef SMSA_DRIVER_INCLUDED
#define SMSA_DRIVER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : smsa_driver.h
//  Description    : This is the driver for the SMSA simulator.
//
//   Author        : Patrick McDaniel
//   Last Modified : Tue Sep 17 07:15:09 EDT 2013
//

// Include Files
#include <stdint.h>

// Project Include Files
#include <smsa.h>

//
// Type Definitions
typedef uint32_t SMSA_VIRTUAL_ADDRESS; // SMSA Driver Virtual Addresses


// Interfaces
int smsa_vmount( void );
	// Mount the SMSA disk array virtual address space

int smsa_vunmount( void );
	// Unmount the SMSA disk array virtual address space

int smsa_vread( SMSA_VIRTUAL_ADDRESS addr, uint32_t len, unsigned char *buf );
	// Read from the SMSA virtual address space

int smsa_vwrite( SMSA_VIRTUAL_ADDRESS addr, uint32_t len, unsigned char *buf );
	// Write to the SMSA virtual address space

#endif
