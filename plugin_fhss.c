/*
 *  Copyright (C) 2017, IOActive www.ioactive.com
 *
 *  This file is part of Plugin_FHSS Blackhat'17
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*

Liquid-SDR http://liquidsdr.org/
LibGSL https://www.gnu.org/software/gsl/

gcc plugin_fhss.c -o pfhss  -lm -lliquid  -lgsl -lgslcblas 

*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <liquid/liquid.h>
#include <complex.h>
#include <sys/stat.h>
#include <getopt.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_sort_float.h>

#include "plugin_fhss.h"


void detectChanFreq( CAPTURE *pcap, float *psd, unsigned int nfft, HOPPING_BURST *pburst, int verify );
int detectLevels( CAPTURE *pcap, float *psd, unsigned int nfft );
int plugin_levels( CAPTURE *pcap, PLUGIN_PARAMS *p_params );
int plugin_levels( CAPTURE *pcap, PLUGIN_PARAMS *p_params );
REPORT * plugin_hopping( CAPTURE *pcap, PLUGIN_PARAMS *p_params );
float complex * load_samples( char *path, int nsamples );
void print_capture_information( CAPTURE *pcap );

#define TRUE    1
#define FALSE   0

#define MAX_CHANNELS 30

/////// PLUGIN_LEVELS
#define MAX_LEVELS  2

#define DB_MIN      20.0 //dB
#define V_FACTOR    2.0  

#define KHZ         1000.0

/////// Globals

CHANNEL aChannels[MAX_CHANNELS]={0};
int g_channelIndex;


void detectChanFreq(CAPTURE *pcap, float *psd, unsigned int nfft, HOPPING_BURST *pburst, int verify)
{
    unsigned int n;
    size_t index_freqs[MAX_FREQS]={0};

    gsl_sort_float_largest_index(index_freqs, MAX_FREQS, psd, 1, nfft);
       

    for ( n = 0; n < MAX_FREQS; n++ )
    {
        // For the current block, store the approx. frequencies having the highest PSD (hopefully Mark, Space);

        // Freq = Center_freq - ( SampleRate/ 2) + ( k* Freq_resolution)
        // k == PSD bin
        // Freq_resolution == Sample rate/ FFT size
        pburst->freqs[ pburst->blockindex ][ n ] = pcap->abs_l_freq +  ( index_freqs[ n ] * pcap->freq_resolution );
       
        // Also save the bin index
        pburst->bin_index[ pburst->blockindex ][ n ] =  index_freqs[ n ]; 
    
    }

    
    return ;
}


int detectLevels(CAPTURE *pcap, float *psd, unsigned int nfft)
{

    float variation_high = 0.0;
    float variation_low = 0.0;
    float temp_variation = 0.0;
    size_t index_freqs[MAX_LEVELS]={0};


    // Get bins with the highest PSD
    gsl_sort_float_largest_index(index_freqs, MAX_LEVELS, psd, 1, nfft);
    variation_high = (psd[index_freqs[0]] + psd[index_freqs[1]])/2 ;

    // Get bins with the smallest PSD
    gsl_sort_float_smallest_index(index_freqs, MAX_LEVELS, psd, 1, nfft);
    variation_low = (psd[index_freqs[0]] + psd[index_freqs[1]])/2 ;
    
    // Difference
    temp_variation = variation_high - variation_low;

    // NOTE: DB_MIN should be 'pHopping->psd_threshold' for flexibility
    // Is there any actual signal here or just noise? 
    
    if( temp_variation > DB_MIN)
    {
        return TRUE;
    }

    return FALSE;

}

int plugin_levels(CAPTURE *pcap, PLUGIN_PARAMS *p_params )
{

    PLUGIN_HOPPING *pHopping;
    int i, n;
    float *psd;
    float complex *dataset;
    float temp_amplitude_init, temp_amplitude_end;
    

    // Init
    pHopping = ( PLUGIN_HOPPING *) p_params->custom_params;

    // IQ Data
    dataset = pcap->dataset;

    /// Allocate the memory to calculate the PSD
    psd = ( float * ) calloc ( pHopping->nfft, sizeof ( float ) );

    // We start processing samples at file offset block_size*2

    for ( i = pcap->block_size * 2; i < pcap->nsamples - pcap->block_size; i += pcap->block_size)
    {
        
        spgramcf q = spgramcf_create_default ( pHopping->nfft );  

        // Push values from the IQ file
        spgramcf_write ( q, dataset + i, pcap->block_size );
        spgramcf_get_psd ( q, psd );
       
        // Did we find any signal or just noise?
        if( detectLevels( pcap, psd, pHopping->nfft) )
        {
            // We've got a signal, let's see where it begins
            for ( n = 0; n < pcap->block_size; n++ )
            {
                // Calculate Amplitude 

                //  We're looking for this scenario
                //
                //       ________________ temp_amplitud_init (noise)
                //  (v) ||                  
                //   |  \/   #######
                //   | =====######## <----temp_amplitud_end  (signal)
                //   |       #######
                //    -------------- (t)
                //    
                //              __________
                // Amplitude = V I^2 + Q^2

                temp_amplitude_init = sqrt( powf( crealf( dataset[ i + n ] ), 2) + powf( cimagf( dataset[ i + n ] ), 2) );
                temp_amplitude_end = sqrt( powf( crealf( dataset[ i + pcap->block_size - n] ), 2) + powf( cimagf( dataset[ i + pcap->block_size - n] ), 2) );         
            
              
                if ( temp_amplitude_end / temp_amplitude_init >= V_FACTOR){

                    // We have found a signal! So time to adjust levels.

                    pHopping->amplitude_valley = temp_amplitude_init;
                    pHopping->amplitude_peak = temp_amplitude_end-(temp_amplitude_end/5.0);

                    printf("[*] Found potential signal => Block: %d Noise: %.4f(v) Signal: %.4f(v)\n",i,pHopping->amplitude_valley,pHopping->amplitude_peak );
                    
                    // Cleanup
                    spgramcf_destroy(q);
                    free(psd);

                    // Save time for the next steps, we consider the first block the one where we have found the first burst.
                    pHopping->first_block = i;

                    return TRUE;
                }
            }
        }
        spgramcf_destroy(q);        
    }
    free(psd);
    
    return FALSE;
}


// Detect FHSS channels 

REPORT * plugin_hopping(CAPTURE *pcap, PLUGIN_PARAMS *p_params )
{

    
  
    int i, x, n, n_local;
    float *psd;
    float complex *dataset;
    float temp_amplitude;
    float timeLow =.0, timeHigh=.0, timeWindow;
    float freq_1, freq_2, freq_center;

    int channelFlag;
    
    PLUGIN_HOPPING *pHopping;
    HOPPING_BURST *pburst;


    // Init
    x = 0;
    pHopping = ( PLUGIN_HOPPING *) p_params->custom_params;
    dataset = pcap->dataset;
    channelFlag =  FALSE;

 
 	/// Allocate the memory to calculate the PSD

 	psd = ( float * ) calloc ( pHopping->nfft, sizeof ( float ) );

    // Allocate HOPPING_BURST
    pburst = ( HOPPING_BURST* ) calloc ( 1, sizeof( HOPPING_BURST ) );
 		
 	  
    // Start at the block where we initially detected the signal

 	for ( i = pHopping->first_block; i < pcap->nsamples - pcap->block_size; i += pcap->block_size)
 	{
 		
 		// Let's look for the amplitude that marks the start of the burst
 		for ( n = 0; n < pcap->block_size; n++ )
 		{
 			temp_amplitude = sqrt( powf( crealf( dataset[ i + n ] ), 2) + powf( cimagf( dataset[ i + n ] ), 2) );
 		     

            // According to the levels previously calculated by detectLevels plugin
 			if ( temp_amplitude >= pHopping->amplitude_peak && timeLow == .0 ) 
 			{
 				// Init time
                timeLow = ( 1.0 / pcap->sample_rate ) * i;
                // We have a channel
 				channelFlag = TRUE;
                aChannels[ g_channelIndex ].offsetLow = i + n;
            
 			} else if (temp_amplitude <= pHopping->amplitude_valley 
                        && timeLow != .0 
                        && timeHigh == .0 )  {  // End of burst based on amplitude.

                // End time
 				timeHigh = ( 1.0 / pcap->sample_rate ) * i;

                // Offset
                aChannels[ g_channelIndex ].offsetHigh = i + n;
 			    
                // We will save the channel and continue searching 
 				channelFlag = FALSE;
               
                //  printf("[+] Start at sample %d Burst [%d] from %.8f to %.8f Total: %.5fs \n", i+n, numberBurst++, timeLow, timeHigh, timeHigh - timeLow);
                aChannels[ g_channelIndex ].timeLow = timeLow; 
                aChannels[ g_channelIndex ].timeHigh = timeHigh;

                timeLow = 0.0;
                timeHigh = 0.0;
                timeWindow = 0.0;

                // We start at the second block
                for( n_local = 1; n_local < pburst->blockindex; n_local++ )
                {
                   
                    size_t freq2index;

                    // Here we detect Mark and Space Freqs to calculate the Center Freq.
                    // Let's get the bin with the hightes PSD and then look for other top freq
                    // situated in other bin that complies with the minimum frequency deviation we choose.
                    freq_2 = pburst->freqs[ n_local ][ x ];
                    freq2index = pburst->bin_index[ n_local ][ x ];

                    do {

                         freq_1 = pburst->freqs[ n_local ][ x++ ];

                     } while( fabs( freq_1 - freq_2 ) < 20.0*KHZ ); // Frequency Deviation (Datasheet http://www.analog.com/media/en/technical-documentation/data-sheets/ADF7023.pdf)

                    
                    // Get Mark and Space Frequencies.
                    if( freq_1 > freq_2 ){

                        freq_center = ( ( freq_1 - freq_2 ) / 2 ) + freq_2;

                    }else{

                        freq_center = ( ( freq_2 - freq_1 ) / 2 ) + freq_1;

                    }

                    // We got FHSS channel's Center Frequency.
                    if(g_channelIndex < MAX_CHANNELS)
                    {
                        aChannels[ g_channelIndex ].centerFreq = freq_center;  
                        g_channelIndex++;
                    }
                    break;
                    
                    
                }
                // Reset Burst's block index.
                pburst->blockindex = 0;
  
 			}	
 		}

         if( channelFlag && pburst->blockindex < MAX_BLOCKS ){

            spgramcf q = spgramcf_create_default ( pHopping->nfft );    
            spgramcf_write ( q, dataset + i, pcap->block_size );
            spgramcf_get_psd ( q, psd );
        
            //Start analyzing blocks within the channel
            detectChanFreq( pcap, psd, pHopping->nfft, pburst, FALSE );

            // New Block for this burst has been analyzed.
            pburst->blockindex++;

            spgramcf_destroy(q);
         }
 	}
    
    return NULL;
}

// Print Usage
void usage()
{
    printf("plugin_fhss [options]\n");
    printf("  h     : print help\n");
    printf("  n     : fft size\n");
    printf("  b     : block size\n");
    printf("  s     : sampling rate\n");
    printf("  d     : iq file\n");
    printf("  x     : center frequency\n");
    printf("Example: ./plugin_fhss -n 2048 -b 8192 -s 8000000 -d sdr/2seconds_0_then_FF.32fc -x 914000000.0\n\n");

}

// Load IQ data from file

float complex * load_samples(char *path, int nsamples )
{

    FILE *fp;
    size_t fSize;
    float complex *buffer;
    struct stat st;
    size_t samplesread;

    buffer = NULL;

    if ( !stat ( path, &st ) ){

        fSize = st.st_size;

        fp = fopen ( path, "rb" );
        
        if ( fSize < nsamples * sizeof ( float complex ) ) {
            
            printf("Error, not enough samples to collect\n");
            exit(1);
        }
        
        buffer = ( float complex* ) calloc( nsamples, sizeof( float complex ) );
        samplesread =	fread ( buffer, sizeof ( float complex ), nsamples, fp );
        printf("\n[+] %zu samples loaded\n",samplesread);
        fclose( fp );
        }
        
    return buffer;
}

// Print information about the capture to be processed.
void print_capture_information(CAPTURE *pcap)
{

    printf("Session =======================================\n");

    printf("1. Sampling Rate: [ %d Hz ]\n2. Block Size: [ %d ]\n3. Freq Resolution: [ %2.3f Hz ] \n4. Low Freq: [ %.3f Hz ]\n5. High Freq: [ %.3f Hz ]\n\n"
            ,pcap->sample_rate
            ,pcap->block_size
            ,pcap->freq_resolution
            ,pcap->abs_l_freq
            ,pcap->abs_h_freq);
    
    return;
 
}

int main(int argc, char **argv) {
    
    char data_file[255]={0};
    int sample_rate;
 	int block_size;
 	int i;

    float center_freq;
    float chan_freqs[MAX_CHANNELS]={.0};

    // options
    unsigned int nfft = 512; 
    int dopt;
  
    
    
    while ( ( dopt = getopt( argc,argv,"hn:b:s:d:x:") ) != EOF ) {
        switch (dopt) {
        case 'h': usage();              return 0;
        // case 'p': amp_p = atof(optarg); break; //option to manually adjust levels
        // case 'v': amp_v = atof(optarg); break; //option to manually adjust levels
        // case 't': psd_threshold = atof(optarg); break; //option to manually adjust levels
        case 'n': nfft          = atoi( optarg );  break;
        case 'b': block_size    = atoi( optarg ); break;
        case 's': sample_rate   = atoi( optarg ); break;
        case 'd': strncpy(data_file,optarg,254);break;
        case 'x': center_freq   = atof( optarg ); break;
        default:
            exit(1);
        }
    }

    // Initialize plugins
    CAPTURE capture_in;
    PLUGIN_PARAMS params_in;
    PLUGIN_HOPPING hopping_params;

    
    capture_in.center_freq = center_freq;
    capture_in.nsamples = sample_rate * 2;

    //Window of samples
    capture_in.block_size = block_size;    
    capture_in.sample_rate = sample_rate;
    

    //Freq resolution and bandwidth
    capture_in.freq_resolution = ( float ) sample_rate / nfft ;
    capture_in.abs_l_freq = center_freq - ( sample_rate / 2 );
    capture_in.abs_h_freq = center_freq + ( sample_rate / 2 );    
    
    
    // Load IQ file
 	capture_in.dataset = load_samples(data_file,capture_in.nsamples);
    params_in.description = "\nIOActive RF Utils - Plugin BlackHat'17 FHSS/2GFSK 0.1 -{Ruben Santamarta}-\n\n";

    // Announce the plugin
    printf("%s", params_in.description);

    // Display information
    print_capture_information(&capture_in);

    // FFT size
    hopping_params.nfft = nfft;

    // Automatically calculated by plugin_levels
    hopping_params.amplitude_peak  = .0; 
    hopping_params.amplitude_valley = .0;

    // To be used for adjusting accuracy (parameter from command-line), so far we are using DB_MIN
    hopping_params.psd_threshold = DB_MIN; 

    params_in.custom_params = ( void* )&hopping_params;
    
    printf("[+] Detecting burst\n");
 	
    // Detect presence of signal and get the amplitude.
    if( plugin_levels(&capture_in,&params_in) ){

    
        printf("\n[+] Detecting Channels...\n");
        //Detect channels
        plugin_hopping( &capture_in, &params_in );
       
        for (i = 0; i< g_channelIndex; i++)
        {
            printf("Channel [%d] =>\tCenter Frequency: %.1f Mhz\t=== Offset: [%d -> %d]\tTime: %f(s) to %f(s)\tSpan: %.5f(s) ===\n",i,aChannels[i].centerFreq/(1000.0*KHZ),aChannels[i].offsetLow,aChannels[i].offsetHigh,aChannels[i].timeLow, aChannels[i].timeHigh, aChannels[i].timeHigh - aChannels[i].timeLow);
        }
   

    }else{
        printf("[!!] Unable to detect burst...");
    }
    


    printf("\n[+] Done.\n");
    return 0;
}