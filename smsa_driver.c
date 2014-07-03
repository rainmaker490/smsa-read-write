////////////////////////////////////////////////////////////////////////////////
//
//  File           : smsa_driver.c
//  Description    : This is the driver for the SMSA simulator.
//
//   Author        : VARUN PATEL
//   Last Modified : OCTOBER 28TH 2013
//

// Project Include Files
#include <smsa_driver.h>
#include <cmpsc311_log.h>

#define ZERO 0


// Global data
// unsigned char *block = NULL;
// Interfaces

// Function that gets the offset
uint32_t getOffset( SMSA_VIRTUAL_ADDRESS addr){
  // temp will be the address "anded" with 0xFF so we can isolate 
  // the necessary bits. return temp.
  uint32_t temp = addr & 0xFF;
  return temp;
}

// Function that returns the Block ID from the address
uint32_t getBlockID(SMSA_VIRTUAL_ADDRESS addr){ 
  // isolate the block ID by getting rid of the un necessary bits
  // shift all the way to the right and return the blockID
  uint32_t temp = addr & 0x0000FF00;
  temp = temp >> 8;
  return temp;
}

// Function that returns the Drum ID from the address
uint32_t getDrumID(SMSA_VIRTUAL_ADDRESS addr){
  // shift the top bits containing the drumID and return
  // error checking makes sure that the drumID is in bounds
  uint32_t temp = addr >> 16;
  if ( (temp > 16) || (temp < 0) ){
    return -1;
  } else {
      return temp;
    }
}

// Function that glues all the IDs together to form OPCODE
uint32_t makeID(SMSA_DISK_COMMAND opcode, uint32_t myDrumId, uint32_t myBlockId){
  // put the drumID into the correct position in the opCode
  uint32_t tempDrumId = myDrumId << 22;
  // next, put the instruction opCode in the correct position in the opCode
  uint32_t tempOpcode = opcode << 26;
  // my blockID will be at the end of the opCode so, just OR the three
  // values, and return the "glued" parts as the opCode
  uint32_t myMakeID = (uint32_t)(tempOpcode | tempDrumId | myBlockId);
  return myMakeID;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function      smsa_vmount
// Description  : Mount the SMSA disk array virtual address space
//
// Inputs       : none
// Outputs      : -1 if failure or 0 if successful

int smsa_vmount( void ) {
  // make the opCode
  uint32_t formatOpCode = makeID(SMSA_MOUNT,ZERO ,ZERO);
  // use smssa_operation to mount
  return smsa_operation(formatOpCode, NULL);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vunmount
// Description  :  Unmount the SMSA disk array virtual address space
//
// Inputs       : none
// Outputs      : -1 if failure or 0 if successful

int smsa_vunmount( void )  {
  // make the opCode
  uint32_t opCode_UnMount = makeID(SMSA_UNMOUNT,ZERO,ZERO);
  // use smsa_operation to unmount
  return smsa_operation(opCode_UnMount, NULL);   
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vread
// Description  : Read from the SMSA virtual address space
//
// Inputs       : addr - the address to read from
//                len - the number of bytes to read
//                buf - the place to put the read bytes
// Outputs      : -1 if failure or 0 if successful

int smsa_vread( SMSA_VIRTUAL_ADDRESS addr, uint32_t len, unsigned char *buf ) {
  // variables - self explanatory from name type
  uint32_t  myOffset = getOffset(addr) ,  opCode_seekDrum, opCode_seekBlock, opCode_diskRead;
  uint32_t myDrum;
  uint32_t myBlock;
  //  char *flag = NULL;
  // temporary buffer to copy bytes into (256 bytes long)
  unsigned char temp[256];

  // get the IDs of the respective components of the address
  myDrum = getDrumID(addr);
  myBlock = getBlockID(addr);
  // set the counter of readBytes to 0 and i to the offset to use to copy bytes
  // in the do-while loop
  // there arent any read bytes when entering the loop
  // we want the head to start reading at the offset, so set counter i
  // at the offset
  uint32_t readBytes = 0 , i =myOffset;

  // Seek Drum so that we are in the correct drum and the read head
  // is in the right position
  opCode_seekDrum = makeID(SMSA_SEEK_DRUM, myDrum, ZERO);
  smsa_operation(opCode_seekDrum, temp);
  
  do {
    // Seek block to current block (temp)
    opCode_seekBlock = makeID(SMSA_SEEK_BLOCK, ZERO, myBlock);
    smsa_operation(opCode_seekBlock, temp);

    // Disk Read function will read the temp
    // read the specified block and drum. then after read, the 
    // head will go to next block and after one iteration of the loop
    // if necessary, will go to next drum.
    opCode_diskRead = makeID(SMSA_DISK_READ, myDrum, myBlock);
    smsa_operation(opCode_diskRead, temp);


    do {
      // copy from temp to buffer
      buf[readBytes] = temp[i];
      readBytes++;
      i++;
    }while (i < 256 && readBytes < len );
    
    // if you run out of the block then move to next block
    // if you run out of room read the full buffer and then go back into
    // loop to read the remaining bytes. Repeat until all is read.
    if(myBlock == 255) {
      myDrum++;
      myBlock = 0;
      opCode_seekDrum = makeID(SMSA_SEEK_DRUM, myDrum, ZERO);
      smsa_operation(opCode_seekDrum, temp);
    } else { myBlock++; }
    // reset counter to 0
    i = 0;
  }while(readBytes < len);
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : smsa_vwrite
// Description  : Write to the SMSA virtual address space
//
// Inputs       : addr - the address to write to
//                len - the number of bytes to write
//                buf - the place to read the read from to write
// Outputs      : -1 if failure or 0 if successful

int smsa_vwrite( SMSA_VIRTUAL_ADDRESS addr, uint32_t len, unsigned char *buf )  {
  if (len == 0) {
    return 0;
  }
  // declare the variables
  uint32_t myBlock, myDrum, offset, i , readBytes, opCode_SeekDrum, opCode_SeekBlock;
  uint32_t opCode_DiskRead, tempBlock, opCode_DiskWrite;
  // int test;
  unsigned char temp[256];
  readBytes = 0;
  i = 0;
  myBlock = getBlockID(addr);
  myDrum = getDrumID(addr);
  offset = getOffset(addr);
  tempBlock = myBlock;
  opCode_SeekDrum = makeID(SMSA_SEEK_DRUM, myDrum, myBlock);
  smsa_operation(opCode_SeekDrum, temp); 
  
  do{
      if (myBlock == tempBlock){
        i = offset;
      } else {
	  i = 0;
	}
      // when running out of bounds, make sure you do some book keeping and
      // update the drum and seek to the top of drum
      if (tempBlock == 256) {
        myDrum++;
        opCode_SeekDrum = makeID(SMSA_SEEK_DRUM , myDrum , ZERO);
        smsa_operation(opCode_SeekDrum, temp);
      }
      
      // opCode_SeekBlock = makeID(SMSA_SEEK_BLOCK, ZERO, myDrum);
      // smsa_operation(opCode_SeekBlock, NULL);
      // Read Disk, and since that 256 bytes are read, move to next block
      // therefore updare the block.
      opCode_SeekBlock = makeID(SMSA_SEEK_BLOCK, myDrum, tempBlock);
      smsa_operation(opCode_SeekBlock, temp);
      opCode_DiskRead = makeID(SMSA_DISK_READ, myDrum, tempBlock);
      smsa_operation(opCode_DiskRead, temp);
      tempBlock++;
      do {
	// write the read bytes into the buffer
        temp[i] = buf[readBytes];
        readBytes++;
        i++;
      } while (i < 256 && readBytes < len);
        // find the Block you want to Write (its going to be the previous Block
	// since the head is on the next position after the Read when entering
	// the loop, the head is automatically moved to the next block
	// so, you want to write to that same block, so therefore, tempBlock-1
	// is used because the read moved the head to the next block
        opCode_SeekBlock = makeID(SMSA_SEEK_BLOCK, myDrum,tempBlock-1);
        smsa_operation(opCode_SeekBlock , temp);
	// write to the Block
	opCode_DiskWrite = makeID(SMSA_DISK_WRITE, myDrum, tempBlock);
	smsa_operation(opCode_DiskWrite, temp);
  } while (readBytes < len);
  return 0;
}
