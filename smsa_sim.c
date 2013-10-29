////////////////////////////////////////////////////////////////////////////////
//
//  File          : smsasim.c
//  Description   : This is the main program for the CMPSC311 programming assignment.
//
//   Author : Patrick McDaniel
//   Last Modified : Sep 10 13:04:32 PDT 2013
//

// Include Files
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Project Includes
#include <smsa.h>
#include <smsa_unittest.h>
#include <smsa_driver.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>

// Defines
#define SMSA_ARGUMENTS "huvl:"
#define USAGE \
	"USAGE: smsa [-h] [-u] [-v] [-l <logfile>] <workload-file>\n" \
	"\n" \
	"where:\n" \
	"    -h - help mode (display this message)\n" \
	"    -u - run the SMSA unit test\n" \
	"    -v - verbose output\n" \
	"    -l - write log messages to the filename <logfile>\n" \
	"\n" \
	"    <workload-file> - file contain the workload to simulate\n" \
	"\n" \

//
// Global Data
int verbose;

//
// Functional Prototypes

int simulate_SMSA( char *wload );

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : main
// Description  : The main function for the SMSA simulator
//
// Inputs       : argc - the number of command line parameters
//                argv - the parameters
// Outputs      : 0 if successful test, -1 if failure

int main( int argc, char *argv[] )
{
	// Local variables
	int ch, verbose = 0, log_initialized = 0, unit_test = 0;

	// Process the command line parameters
	while ((ch = getopt(argc, argv, SMSA_ARGUMENTS)) != -1) {

		switch (ch) {
		case 'h': // Help, print usage
			fprintf( stderr, USAGE );
			return( -1 );

		case 'v': // Verbose Flag
			verbose = 1;
			break;

		case 'u': // Run the unit test (instead of running the program)
			unit_test = 1;
			break;

		case 'l': // Set the log filename
			initializeLogWithFilename( optarg );
			log_initialized = 1;
			break;

		default:  // Default (unknown)
			fprintf( stderr, "Unknown command line option (%c), aborting.\n", ch );
			return( -1 );
		}
	}

	// Setup the log as needed
	if ( ! log_initialized ) {
		initializeLogWithFilehandle( CMPSC311_LOG_STDERR );
	}
	if ( verbose ) {
		enableLogLevels( LOG_INFO_LEVEL );
	}

	// If running the UNIT test
	if ( unit_test ) {

		// Increase log level, run the UNIT tests
		enableLogLevels( LOG_INFO_LEVEL );
		smsa_unit_test();
		smsa_vread_unit_test();


	} else {

		// The filename should be the next option
		if ( optind >= argc ) {
			// No filename
			fprintf( stderr, "Missing command line parameters, use -h to see usage, aborting.\n" );
			return( -1 );
		}

		// Run the simulation
		if ( simulate_SMSA(argv[optind]) == 0 ) {

			// Program completed successfully
			logMessage( LOG_INFO_LEVEL, "SMSA simulation completed successfully.\n\n" );

		} else {

			// Program completed unsuccessfully
			logMessage( LOG_INFO_LEVEL, "SMSA simulation failed.\n\n" );

		}
	}

	// Return successfully
	return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : simulate_SMSA
// Description  : The main control loop for the processing of the SMSA sim
//
// Inputs       : wload - the name of the workload file
// Outputs      : 0 if successful test, -1 if failure

int simulate_SMSA( char *wload ) {

	// Local variables
	char line[256], cmd[32];
	unsigned char buf[SMSA_MAXIMUM_RDWR_SIZE], sig[CMPSC311_HASH_LENGTH], sigstr[CMPSC311_HASH_LENGTH*4];
	FILE *fhandle = NULL;
	uint32_t addr, len, ch, slen;
	int i, j, err;

	// Open the workload file
	if ( (fhandle=fopen(wload, "r")) == NULL ) {
		logMessage( LOG_ERROR_LEVEL, "Failure opening the workload file [%s], error: %s.\n",
			wload, strerror(errno) );
		return( -1 );
	}

	// While file not done
	while (!feof(fhandle)) {

		// Get the line and bail out on fail
		if ( fgets(line, 256, fhandle) != NULL ) {

			// Check for mount
			if ( strncmp(SMSA_WORKLOAD_MOUNT,line,strlen(SMSA_WORKLOAD_MOUNT)) == 0 ) {
				logMessage( LOG_INFO_LEVEL, "Calling virtual driver mount ");
				err = smsa_vmount();
			}

			// Check for mount
			else if ( strncmp(SMSA_WORKLOAD_UNMOUNT,line,strlen(SMSA_WORKLOAD_UNMOUNT)) == 0 ) {
				logMessage( LOG_INFO_LEVEL, "Calling virtual driver unmount ");
				err = smsa_vunmount();
			}

			// Check for mount
			else if ( strncmp(SMSA_WORKLOAD_SIGNALL,line,strlen(SMSA_WORKLOAD_SIGNALL)) == 0 ) {
				logMessage( LOG_INFO_LEVEL, "Computing signatures on the array.");

				// Now just test the disk block signature generation
				for ( i=0; i<SMSA_DISK_ARRAY_SIZE; i++ ) {
					for ( j=0; j<SMSA_MAX_BLOCK_ID; j++ ) {
						SMSABlockSign( i, j );
					}
				}
			}

			else {

				// Parse out the command
				if ( sscanf( line, "%7s %7u %4u %3u", cmd, &addr, &len, &ch ) != 4 ) {
					logMessage( LOG_ERROR_LEVEL, "Error parsing virtual command [%s\n]", line );
					fclose( fhandle );
					return( -1 );
				}

				// Check for read
				if ( strncmp(SMSA_WORKLOAD_READ, cmd, strlen(SMSA_WORKLOAD_READ)) == 0 ) {
					logMessage( LOG_INFO_LEVEL, "Calling virtual driver read (addr=%x, len=%u)", addr, len);

					// Do the read, fingerprint the returned buffer so we can validate
					if ( !(err = smsa_vread( addr, len, buf )) ) {

						// Setup and do signature
						slen = CMPSC311_HASH_LENGTH;
						memset( sig, 0x0, slen );
						if ( generate_md5_signature( buf, len, sig, &slen) ) {
							logMessage( LOG_ERROR_LEVEL, "SIM Signature failed (%lu)", addr );
							return( -1 );
						}

						// Log the signature
						bufToString( sig, slen, sigstr, CMPSC311_HASH_LENGTH*4 );
						logMessage( LOG_INFO_LEVEL, "READ SIG : %lu len %lu - %s", addr, len, sigstr );

					} else {
						// Print out error
						logMessage( LOG_ERROR_LEVEL, "Read failed (%lu,len=%lu)", addr, len );
					}
				}

				// Check for write
				else if ( strncmp(SMSA_WORKLOAD_WRITE, cmd, strlen(SMSA_WORKLOAD_WRITE)) == 0 ) {

					// Now setup the buffer and make the call
					logMessage( LOG_INFO_LEVEL, "Calling virtual driver write (addr=%x, len=%u, ch=%u)", addr, len, ch);
					memset( buf, ch, len );
					err = smsa_vwrite( addr, len, buf );
				}

				else {

					// WTF? I don't know what this is
					logMessage( LOG_ERROR_LEVEL, "Unknown virtual command, aborting [%s]", cmd );
					fclose( fhandle );
					return( -1 );
				}
			}

			// Check for the virtual level failing
			if ( err ) {
				logMessage( LOG_ERROR_LEVEL, "Virtual array failed, aborting [%d]", err );
				fclose( fhandle );
				return( -1 );
			}
		}
	}
  
	// Close the workload file
	fclose( fhandle );

	// Return successfully
	return( 0 );
}
