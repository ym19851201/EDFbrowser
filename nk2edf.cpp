/*
***************************************************************************
*
* Author: Teunis van Beelen
*
* Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Teunis van Beelen
*
* Email: teuniz@gmail.com
*
***************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***************************************************************************
*/



#include "nk2edf.h"



#define ELECTRODE_TAG "[ELECTRODE]"
#define ELECTRODE_UNTAG "["
#define ELECTRODE_NAME_MAXLEN 256

#define ANNOT_TRACKSIZE 54

int main(int argc, char* args[])
{
  FILE *inputfile=NULL,
       *outputfile=NULL,
       *logfile=NULL,
       *pntfile=NULL;

  int i, j, k,
      error,
      ctl_block_cnt,
      datablock_cnt,
      total_blocks,
      edfplus,
      n_logs=0,
      n_sublogs=0,
      total_logs=0,
      n_logblocks=0,
      ctlblock_address,
      wfmblock_address,
      logblock_address,
      read_subevents=0;

  char txt_string[2048],
       path[MAX_PATH_LENGTH],
       outputpath[MAX_PATH_LENGTH],
       logfilepath[MAX_PATH_LENGTH],
       pntfilepath[MAX_PATH_LENGTH],
       *log_buf=NULL,
       *sublog_buf=NULL,
       scratchpad[256];



//  edfplus = checkBox1->checkState();
  edfplus = true;

  total_elapsed_time = 0;

  for(int count = 1; count < argc; count++) {
    strcpy(path, args[count]);

    if(!strcmp(path, ""))
    {
      return -1;
    }


    inputfile = fopeno(path, "rb");
    if(inputfile==NULL)
    {
      snprintf(txt_string, 2048, "can not open file %s for reading.\n", path);
      return -1;
    }

    /***************** check if the EEG file is valid ******************************/

    rewind(inputfile);
    if(fread(scratchpad, 16, 1, inputfile)!=1)
    {
      fclose(inputfile);
      return -1;
    }
    scratchpad[16] = 0;
    if(check_device(scratchpad))
    {
      snprintf(txt_string, 2048, "error, deviceblock has unknown signature: \"%s\"\n", scratchpad);
      fclose(inputfile);
      return -1;
    }
    fseeko(inputfile, 0x0081LL, SEEK_SET);
    if(fread(scratchpad, 16, 1, inputfile)!=1)
    {
      fclose(inputfile);
      return -1;
    }
    scratchpad[16] = 0;
    if(check_device(scratchpad))
    {
      snprintf(txt_string, 2048, "error, controlblock has unknown signature: \"%s\"\n", scratchpad);
      fclose(inputfile);
      return -1;
    }
    fseeko(inputfile, 0x17feLL, SEEK_SET);
    if(fgetc(inputfile)!=0x01)
    {
      snprintf(txt_string, 2048, "error, waveformdatablock has wrong signature.\n");
      fclose(inputfile);
      return -1;
    }

    /************************* read logs **********************************************/

    if(edfplus)
    {
      strncpy(logfilepath, path, MAX_PATH_LENGTH);
      remove_extension_from_filename(logfilepath);
      strcat(logfilepath, ".LOG");
      logfile = fopeno(logfilepath, "rb");
      if(logfile==NULL)
      {
        remove_extension_from_filename(logfilepath);
        strcat(logfilepath, ".log");
        logfile = fopeno(logfilepath, "rb");
        if(logfile==NULL)
        {
          snprintf(txt_string, 2048, "Can not open file %s for reading,\n"
              "if there is no .log file you can try to create an EDF file instead of EDF+.\n",
              logfilepath);
          fclose(inputfile);
          return -1;
        }
      }

      rewind(logfile);
      if(fread(scratchpad, 16, 1, logfile)!=1)
      {
        fclose(logfile);
        fclose(inputfile);
        return -1;
      }
      scratchpad[16] = 0;
      if(check_device(scratchpad))
      {
        snprintf(txt_string, 2048, "error, .log file has unknown signature: \"%s\"\n", scratchpad);
        fclose(logfile);
        fclose(inputfile);
        return -1;
      }

      fseeko(logfile, 0x0091LL, SEEK_SET);
      n_logblocks = fgetc(logfile);
      log_buf = (char *)calloc(1, n_logblocks * 11521);
      if(log_buf==NULL)
      {
        fclose(logfile);
        fclose(inputfile);
        return -1;
      }
      sublog_buf = (char *)calloc(1, n_logblocks * 11521);
      if(sublog_buf==NULL)
      {
        fclose(logfile);
        fclose(inputfile);
        free(log_buf);
        return -1;
      }

      read_subevents = 1;

      total_logs = 0;

      for(i=0; i<n_logblocks; i++)
      {
        fseeko(logfile, (long long)(0x0092 + (i * 20)), SEEK_SET);
        if(fread((char *)(&logblock_address), 4, 1, logfile)!=1)
        {
          fclose(inputfile);
          fclose(logfile);
          free(log_buf);
          free(sublog_buf);
          return -1;
        }
        fseeko(logfile, (long long)(logblock_address + 0x0012), SEEK_SET);
        n_logs = fgetc(logfile);
        fseeko(logfile, (long long)(logblock_address + 0x0014), SEEK_SET);
        if(fread(log_buf + (total_logs * 45), n_logs * 45, 1, logfile)!=1)
        {
          fclose(inputfile);
          fclose(logfile);
          free(log_buf);
          free(sublog_buf);
          return -1;
        }

        if(read_subevents)
        {
          if(fseeko(logfile, 0x0092LL + ((i + 22) * 20) , SEEK_SET))
          {
            read_subevents = 0;
          }
          else
          {
            if(fread((char *)(&logblock_address), 4, 1, logfile)!=1)
            {
              read_subevents = 0;
            }
            else
            {
              if(fseeko(logfile, logblock_address + 0x0012LL, SEEK_SET))
              {
                read_subevents = 0;
              }
              else
              {
                n_sublogs = fgetc(logfile);
                if(n_sublogs != n_logs)
                {
                  read_subevents = 0;
                }
                else
                {
                  if(fseeko(logfile, logblock_address + 0x0014LL, SEEK_SET))
                  {
                    read_subevents = 0;
                  }
                  else
                  {
                    if(fread(sublog_buf + (total_logs * 45), n_sublogs * 45, 1, logfile)!=1)
                    {
                      read_subevents = 0;
                    }
                  }
                }
              }
            }
          }
        }

        total_logs += n_logs;
      }

      for(i=0; i<total_logs; i++)
      {
        for(j=0; j<20; j++)
        {
          if(((unsigned char *)log_buf)[(i * 45) + j]<32)  log_buf[(i * 45) + j] = ' ';
        }

        latin1_to_utf8(log_buf + (i * 45), 20);

        if(read_subevents)
        {
          strncpy(log_buf + (i * 45) + 26, sublog_buf + (i * 45) + 24, 6);
        }
      }

      /************************* check pntfile **********************************************/

      strncpy(pntfilepath, path, MAX_PATH_LENGTH);
      remove_extension_from_filename(logfilepath);
      strcat(logfilepath, ".PNT");
      pntfile = fopeno(pntfilepath, "rb");
      if(pntfile==NULL)
      {
        remove_extension_from_filename(logfilepath);
        strcat(logfilepath, ".pnt");
        pntfile = fopeno(pntfilepath, "rb");
        if(pntfile==NULL)
        {
          snprintf(txt_string, 2048, "Can not open file %s for reading,\n"
              "if there is no .pnt file you can try to create an EDF file instead of EDF+.\n",
              pntfilepath);
          fclose(logfile);
          fclose(inputfile);
          free(log_buf);
          free(sublog_buf);
          return -1;
        }
      }

      rewind(pntfile);
      if(fread(scratchpad, 16, 1, pntfile)!=1)
      {
        fclose(pntfile);
        fclose(logfile);
        fclose(inputfile);
        free(log_buf);
        free(sublog_buf);
        return -1;
      }
      scratchpad[16] = 0;
      if(check_device(scratchpad))
      {
        snprintf(txt_string, 2048, "error, .pnt file has unknown signature: \"%s\"\n", scratchpad);
        fclose(pntfile);
        fclose(logfile);
        fclose(inputfile);
        free(log_buf);
        free(sublog_buf);
        return -1;
      }
    }

    /***************** initialize labels **************************************/

    for(i=0; i<256; i++)
    {
      strcpy(labels[i], "-               ");
    }

    strcpy(labels[0],   "EEG FP1         ");
    strcpy(labels[1],   "EEG FP2         ");
    strcpy(labels[2],   "EEG F3          ");
    strcpy(labels[3],   "EEG F4          ");
    strcpy(labels[4],   "EEG C3          ");
    strcpy(labels[5],   "EEG C4          ");
    strcpy(labels[6],   "EEG P3          ");
    strcpy(labels[7],   "EEG P4          ");
    strcpy(labels[8],   "EEG O1          ");
    strcpy(labels[9],   "EEG O2          ");
    strcpy(labels[10],  "EEG F7          ");
    strcpy(labels[11],  "EEG F8          ");
    strcpy(labels[12],  "EEG T3          ");
    strcpy(labels[13],  "EEG T4          ");
    strcpy(labels[14],  "EEG T5          ");
    strcpy(labels[15],  "EEG T6          ");
    strcpy(labels[16],  "EEG FZ          ");
    strcpy(labels[17],  "EEG CZ          ");
    strcpy(labels[18],  "EEG PZ          ");
    strcpy(labels[19],  "EEG E           ");
    strcpy(labels[20],  "EEG PG1         ");
    strcpy(labels[21],  "EEG PG2         ");
    strcpy(labels[22],  "EEG A1          ");
    strcpy(labels[23],  "EEG A2          ");
    strcpy(labels[24],  "EEG T1          ");
    strcpy(labels[25],  "EEG T2          ");
    for(i=26; i<35; i++)
    {
      sprintf(labels[i], "EEG X%i          ", i - 25);
    }
    strcpy(labels[35],  "EEG X10         ");
    strcpy(labels[36],  "EEG X11         ");
    for(i=42; i<74; i++)
    {
      sprintf(labels[i], "DC%02i            ", i - 41);
    }
    strcpy(labels[74],  "EEG BN1         ");
    strcpy(labels[75],  "EEG BN2         ");
    strcpy(labels[76],  "EEG Mark1       ");
    strcpy(labels[77],  "EEG Mark2       ");
    strcpy(labels[100], "EEG X12/BP1     ");
    strcpy(labels[101], "EEG X13/BP2     ");
    strcpy(labels[102], "EEG X14/BP3     ");
    strcpy(labels[103], "EEG X15/BP4     ");
    for(i=104; i<188; i++)
    {
      sprintf(labels[i], "EEG X%i         ", i - 88);
    }
    for(i=188; i<254; i++)
    {
      sprintf(labels[i], "EEG X%i        ", i - 88);
    }
    strcpy(labels[255], "Z               ");

    /***************** start conversion **************************************/

    total_blocks = 0;

    fseeko(inputfile, 0x0091LL, SEEK_SET);
    ctl_block_cnt = fgetc(inputfile);
    if(ctl_block_cnt==EOF)
    {
      fclose(inputfile);
      if(edfplus)
      {
        fclose(logfile);
        free(log_buf);
        free(sublog_buf);
        fclose(pntfile);
      }
      return -1;
    }

    for(i=0; i<ctl_block_cnt; i++)
    {
      fseeko(inputfile, (long long)(0x0092 + (i * 20)), SEEK_SET);
      if(fread((char *)(&ctlblock_address), 4, 1, inputfile)!=1)
      {
        fclose(inputfile);
        if(edfplus)
        {
          fclose(logfile);
          free(log_buf);
          free(sublog_buf);
          fclose(pntfile);
        }
        return -1;
      }
      fseeko(inputfile, (long long)(ctlblock_address + 17), SEEK_SET);
      datablock_cnt = fgetc(inputfile);
      if(datablock_cnt==EOF)
      {
        fclose(inputfile);
        if(edfplus)
        {
          fclose(logfile);
          free(log_buf);
          free(sublog_buf);
          fclose(pntfile);
        }
        return -1;
      }

      for(j=0; j<datablock_cnt; j++)
      {
        fseeko(inputfile, (long long)(ctlblock_address + (j * 20) + 18), SEEK_SET);
        if(fread((char *)(&wfmblock_address), 4, 1, inputfile)!=1)
        {
          fclose(inputfile);
          if(edfplus)
          {
            fclose(logfile);
            free(log_buf);
            free(sublog_buf);
            fclose(pntfile);
          }
          return -1;
        }

        /********************************************************************/

        strcpy(outputpath, path);
        if(edfplus)  sprintf(outputpath + strlen(path) - 4, "_%u-%u+.edf", i + 1, j + 1);
        else  sprintf(outputpath + strlen(path) - 4, "_%u-%u.edf", i + 1, j + 1);

        outputfile = fopeno(outputpath, "wb");
        if(outputfile==NULL)
        {
          snprintf(txt_string, 2048, "can not open file %s for writing.\n", outputpath);
          fclose(inputfile);
          if(edfplus)
          {
            fclose(logfile);
            free(log_buf);
            free(sublog_buf);
            fclose(pntfile);
          }
          return -1;
        }


        error = convert_nk2edf(inputfile, outputfile, pntfile, wfmblock_address, edfplus, total_logs, log_buf, read_subevents);
        if(error==0)
        {
          snprintf(txt_string, 2048, "to %s", outputpath);
        }

        if(fclose(outputfile))
        {
          fclose(inputfile);
          if(edfplus)
          {
            fclose(logfile);
            fclose(pntfile);
            free(log_buf);
            free(sublog_buf);
          }
          return -1;
        }

        if(error)
        {
          if(edfplus)
          {
            fclose(logfile);
            fclose(pntfile);
            free(log_buf);
            free(sublog_buf);
          }
          return -1;
        }

        total_blocks++;

        /**************************************************************/
      }
    }

    if(edfplus)
    {
      free(log_buf);
      free(sublog_buf);
    }

    if(edfplus)  snprintf(txt_string, 2048, "Converted %u waveformblock(s) successfully to EDF+.\n", total_blocks);
    else  snprintf(txt_string, 2048, "Converted %u waveformblock(s) successfully to EDF.\n", total_blocks);

  }
}




int convert_nk2edf(FILE *inputfile, FILE *outputfile, FILE *pntfile,  int offset, int edfplus, int n_logs, char *log_buf, int read_subevents)
{
  int i, j, k, p,
      temp,
      channels,
      samplefrequency,
      record_duration,
      raster,
      record_size,
      max_buf_records,
      bufsize,
      records_in_buf,
      seconds,
      deci_seconds,
      left_records,
      elapsed_time,
      error,
      records_written,
      progress_steps,
      n_log_processed;

  char *buf,
       *annotations,
       scratchpad[48];

  /************************* filter events ******************************************/

  for(i=0; i<n_logs; i++)
  {
    elapsed_time = 36000 * (log_buf[(i * 45) + 20] - 48);
    elapsed_time += 3600 * (log_buf[(i * 45) + 21] - 48);
    elapsed_time += 600 * (log_buf[(i * 45) + 22] - 48);
    elapsed_time += 60 * (log_buf[(i * 45) + 23] - 48);
    elapsed_time += 10 * (log_buf[(i * 45) + 24] - 48);
    elapsed_time += log_buf[(i * 45) + 25] - 48;
    if(elapsed_time>=total_elapsed_time) break;
  }
  log_buf += i * 45;
  n_logs -= i;

  /************************* write EDF-header ***************************************/

  rewind(outputfile);

  fprintf(outputfile, "0       ");

  if(edfplus)
  {
    error = 0;
    fseeko(pntfile, 0x0604LL, SEEK_SET);
    if(fread(scratchpad, 10, 1, pntfile)!=1)
    {
      return(2);
    }
    scratchpad[10] = 0;
    latin1_to_ascii(scratchpad, strlen(scratchpad));
    for(i=0; i<10; i++)
    {
      if(scratchpad[i]==0)  break;
      if(scratchpad[i]==' ')  scratchpad[i] = '_';
    }
    if(i)
    {
      p = i;
      if(fwrite(scratchpad, i, 1, outputfile)!=1)
      {
        return(3);
      }
    }
    else
    {
      fputc('X', outputfile);
      p = 1;
    }
    fputc(' ', outputfile);
    p++;

    fseeko(pntfile, 0x064aLL, SEEK_SET);
    if(fread(scratchpad, 6, 1, pntfile)!=1)
    {
      return(2);
    }
    if(!strncmp(scratchpad, "Male", 4))  fputc('M', outputfile);
    else
    {
      if(!strncmp(scratchpad, "Female", 6))  fputc('F', outputfile);
      else  fputc('X', outputfile);
    }
    p++;
    fputc(' ', outputfile);
    p++;

    fseeko(pntfile, 0x0668LL, SEEK_SET);
    if(fread(scratchpad, 2, 1, pntfile)!=1)
    {
      return(2);
    }
    scratchpad[2] = 0;
    temp = atoi(scratchpad);
    if((temp<1)||(temp>31))  error = 1;
    for(i=0; i<2; i++)
    {
      if((scratchpad[i]<'0')||(scratchpad[i]>'9'))
      {
        error = 1;
        break;
      }
    }
    fseeko(pntfile, 0x0665LL, SEEK_SET);
    if(fread(scratchpad, 2, 1, pntfile)!=1)
    {
      return(2);
    }
    scratchpad[2] = 0;
    temp = atoi(scratchpad);
    if((temp<1)||(temp>12))  error = 1;
    fseeko(pntfile, 0x0660LL, SEEK_SET);
    if(fread(scratchpad, 4, 1, pntfile)!=1)
    {
      return(2);
    }
    scratchpad[4] = 0;
    temp = atoi(scratchpad);
    if((temp<1)||(temp>9999))  error = 1;
    for(i=0; i<4; i++)
    {
      if((scratchpad[i]<'0')||(scratchpad[i]>'9'))
      {
        error = 1;
        break;
      }
    }

    if(error)
    {
      fputc('X', outputfile);
      p++;
    }
    else
    {
      fseeko(pntfile, 0x0668LL, SEEK_SET);
      if(fread(scratchpad, 2, 1, pntfile)!=1)
      {
        return(2);
      }
      scratchpad[2] = 0;
      temp = atoi(scratchpad);
      if((temp<1)||(temp>31))
      {
        snprintf(scratchpad, 48, "01");
        error = 1;
      }
      for(i=0; i<2; i++)
      {
        if((scratchpad[i]<'0')||(scratchpad[i]>'9'))
        {
          snprintf(scratchpad, 48, "01");
          error = 1;
          break;
        }
      }
      if(fwrite(scratchpad, 2, 1, outputfile)!=1)
      {
        return(3);
      }
      p += 2;
      fputc('-', outputfile);
      p++;
      fseeko(pntfile, 0x0665LL, SEEK_SET);
      if(fread(scratchpad, 2, 1, pntfile)!=1)
      {
        return(2);
      }
      scratchpad[2] = 0;
      temp = atoi(scratchpad);
      switch(temp)
      {
        case  1 : strcpy(scratchpad, "JAN");
                  break;
        case  2 : strcpy(scratchpad, "FEB");
                  break;
        case  3 : strcpy(scratchpad, "MAR");
                  break;
        case  4 : strcpy(scratchpad, "APR");
                  break;
        case  5 : strcpy(scratchpad, "MAY");
                  break;
        case  6 : strcpy(scratchpad, "JUN");
                  break;
        case  7 : strcpy(scratchpad, "JUL");
                  break;
        case  8 : strcpy(scratchpad, "AUG");
                  break;
        case  9 : strcpy(scratchpad, "SEP");
                  break;
        case 10 : strcpy(scratchpad, "OCT");
                  break;
        case 11 : strcpy(scratchpad, "NOV");
                  break;
        case 12 : strcpy(scratchpad, "DEC");
                  break;
        default : strcpy(scratchpad, "JAN");
                  error = 1;
                  break;
      }
      if(fwrite(scratchpad, 3, 1, outputfile)!=1)
      {
        return(3);
      }
      p += 3;
      fputc('-', outputfile);
      p++;
      fseeko(pntfile, 0x0660LL, SEEK_SET);
      if(fread(scratchpad, 4, 1, pntfile)!=1)
      {
        return(2);
      }
      scratchpad[4] = 0;
      temp = atoi(scratchpad);
      if((temp<1)||(temp>9999))
      {
        snprintf(scratchpad, 48, "1800");
        error = 1;
      }
      for(i=0; i<4; i++)
      {
        if((scratchpad[i]<'0')||(scratchpad[i]>'9'))
        {
          snprintf(scratchpad, 48, "1800");
          error = 1;
          break;
        }
      }
      if(fwrite(scratchpad, 4, 1, outputfile)!=1)
      {
        return(3);
      }
      p += 4;
    }

    fputc(' ', outputfile);
    p++;

    fseeko(pntfile, 0x062eLL, SEEK_SET);
    if(fread(scratchpad, 20, 1, pntfile)!=1)
    {
      return(2);
    }
    scratchpad[20] = 0;
    latin1_to_ascii(scratchpad, strlen(scratchpad));
    for(i=0; i<20; i++)
    {
      if(scratchpad[i]==0)  break;
      if(scratchpad[i]==' ')  scratchpad[i] = '_';
    }
    if(i)
    {
      p += i;
      if(fwrite(scratchpad, i, 1, outputfile)!=1)
      {
        return(3);
      }
    }
    else
    {
      fputc('X', outputfile);
      p++;
    }

    for(i=0; i<80-p; i++)  fputc(' ', outputfile);

    if(fwrite("Startdate ", 10, 1, outputfile)!=1)
    {
      return(3);
    }
    p = 10;
    error = 0;
    fseeko(pntfile, 0x0046LL, SEEK_SET);
    if(fread(scratchpad, 2, 1, pntfile)!=1)
    {
      return(2);
    }
    scratchpad[2] = 0;
    temp = atoi(scratchpad);
    if((temp<1)||(temp>31))  error = 1;
    for(i=0; i<2; i++)
    {
      if((scratchpad[i]<'0')||(scratchpad[i]>'9'))
      {
        error = 1;
        break;
      }
    }
    fseeko(pntfile, 0x0044LL, SEEK_SET);
    if(fread(scratchpad, 2, 1, pntfile)!=1)
    {
      return(2);
    }
    scratchpad[2] = 0;
    temp = atoi(scratchpad);
    if((temp<1)||(temp>12))  error = 1;
    fseeko(pntfile, 0x0040LL, SEEK_SET);
    if(fread(scratchpad, 4, 1, pntfile)!=1)
    {
      return(2);
    }
    scratchpad[4] = 0;
    temp = atoi(scratchpad);
    if((temp<1970)||(temp>9999))  error = 1;
    for(i=0; i<4; i++)
    {
      if((scratchpad[i]<'0')||(scratchpad[i]>'9'))
      {
        error = 1;
        break;
      }
    }

    if(error)
    {
      fputc('X', outputfile);
      p++;
    }
    else
    {
      fseeko(pntfile, 0x0046LL, SEEK_SET);
      if(fread(scratchpad, 2, 1, pntfile)!=1)
      {
        return(2);
      }
      scratchpad[2] = 0;
      temp = atoi(scratchpad);
      if((temp<1)||(temp>31))  snprintf(scratchpad, 48, "01");
      for(i=0; i<2; i++)
      {
        if((scratchpad[i]<'0')||(scratchpad[i]>'9'))
        {
          snprintf(scratchpad, 48, "01");
          break;
        }
      }
      if(fwrite(scratchpad, 2, 1, outputfile)!=1)
      {
        return(3);
      }
      fputc('-', outputfile);
      fseeko(pntfile, 0x0044LL, SEEK_SET);
      if(fread(scratchpad, 2, 1, pntfile)!=1)
      {
        return(2);
      }
      scratchpad[2] = 0;
      temp = atoi(scratchpad);
      switch(temp)
      {
        case  1 : strcpy(scratchpad, "JAN");
                  break;
        case  2 : strcpy(scratchpad, "FEB");
                  break;
        case  3 : strcpy(scratchpad, "MAR");
                  break;
        case  4 : strcpy(scratchpad, "APR");
                  break;
        case  5 : strcpy(scratchpad, "MAY");
                  break;
        case  6 : strcpy(scratchpad, "JUN");
                  break;
        case  7 : strcpy(scratchpad, "JUL");
                  break;
        case  8 : strcpy(scratchpad, "AUG");
                  break;
        case  9 : strcpy(scratchpad, "SEP");
                  break;
        case 10 : strcpy(scratchpad, "OCT");
                  break;
        case 11 : strcpy(scratchpad, "NOV");
                  break;
        case 12 : strcpy(scratchpad, "DEC");
                  break;
        default : strcpy(scratchpad, "JAN");
                  break;
      }
      if(fwrite(scratchpad, 3, 1, outputfile)!=1)
      {
        return(3);
      }
      fputc('-', outputfile);
      fseeko(pntfile, 0x0040LL, SEEK_SET);
      if(fread(scratchpad, 4, 1, pntfile)!=1)
      {
        return(2);
      }
      scratchpad[4] = 0;
      temp = atoi(scratchpad);
      if((temp<1)||(temp>9999))  snprintf(scratchpad, 48, "1800");
      for(i=0; i<4; i++)
      {
        if((scratchpad[i]<'0')||(scratchpad[i]>'9'))
        {
          snprintf(scratchpad, 48, "1800");
          break;
        }
      }
      if(fwrite(scratchpad, 4, 1, outputfile)!=1)
      {
        return(3);
      }
      p += 11;
    }

    fputc(' ', outputfile);
    p++;

    fseeko(pntfile, 0x061cLL, SEEK_SET);
    if(fread(scratchpad, 10, 1, pntfile)!=1)
    {
      return(2);
    }
    scratchpad[10] = 0;
    latin1_to_ascii(scratchpad, strlen(scratchpad));
    for(i=0; i<10; i++)
    {
      if(scratchpad[i]==0)  break;
      if(scratchpad[i]==' ')  scratchpad[i] = '_';
    }
    if(i)
    {
      p += i;
      if(fwrite(scratchpad, i, 1, outputfile)!=1)
      {
        return(3);
      }
    }
    else
    {
      fputc('X', outputfile);
      p++;
    }

    fputc(' ', outputfile);
    p++;

    fseeko(pntfile, 0x06aaLL, SEEK_SET);
    if(fread(scratchpad, 20, 1, pntfile)!=1)
    {
      return(2);
    }
    scratchpad[20] = 0;
    latin1_to_ascii(scratchpad, strlen(scratchpad));
    for(i=0; i<20; i++)
    {
      if(scratchpad[i]==0)  break;
      if(scratchpad[i]==' ')  scratchpad[i] = '_';
    }
    if(i)
    {
      p += i;
      if(fwrite(scratchpad, i, 1, outputfile)!=1)
      {
        return(3);
      }
    }
    else
    {
      fputc('X', outputfile);
      p++;
    }

    fputc(' ', outputfile);
    p++;

    if(fwrite("Nihon_Kohden_", 13, 1, outputfile)!=1)
    {
      return(3);
    }
    p += 13;
    rewind(inputfile);
    if(fread(scratchpad, 16, 1, inputfile)!=1)
    {
      return(2);
    }
    scratchpad[16] = 0;
    latin1_to_ascii(scratchpad, strlen(scratchpad));
    for(i=0; i<16; i++)
    {
      if(scratchpad[i]==0)  break;
      if(scratchpad[i]==' ')  scratchpad[i] = '_';
    }
    if(fwrite(scratchpad, i, 1, outputfile)!=1)
    {
      return(3);
    }
    p += i;

    for(i=0; i<80-p; i++)  fputc(' ', outputfile);
  }
  else
  {
    fseeko(inputfile, 0x004fLL, SEEK_SET);
    if(fread(scratchpad, 32, 1, inputfile)!=1)
    {
      return(2);
    }
    scratchpad[32] = 0;
    latin1_to_ascii(scratchpad, strlen(scratchpad));
    for(i=0; i<32; i++)
    {
      if(scratchpad[i]==0)  break;
    }
    p = 80 - i;
    if(fwrite(scratchpad, i, 1, outputfile)!=1)
    {
      return(3);
    }
    for(i=0; i<p; i++)  fputc(' ', outputfile);

    if(fwrite("Nihon Kohden ", 13, 1, outputfile)!=1)
    {
      return(3);
    }
    rewind(inputfile);
    if(fread(scratchpad, 16, 1, inputfile)!=1)
    {
      return(2);
    }
    scratchpad[16] = 0;
    latin1_to_ascii(scratchpad, strlen(scratchpad));
    for(i=0; i<16; i++)
    {
      if(scratchpad[i]==0)  break;
    }
    p = 67 - i;
    if(fwrite(scratchpad, i, 1, outputfile)!=1)
    {
      return(3);
    }
    for(i=0; i<p; i++)  fputc(' ', outputfile);
  }

  fseeko(inputfile, (long long)(0x0016 + offset), SEEK_SET);
  temp = fgetc(inputfile);
  fprintf(outputfile, "%02u.", ((temp >> 4) * 10) + (temp & 15));
  fseeko(inputfile, (long long)(0x0015 + offset), SEEK_SET);
  temp = fgetc(inputfile);
  fprintf(outputfile, "%02u.", ((temp >> 4) * 10) + (temp & 15));
  fseeko(inputfile, (long long)(0x0014 + offset), SEEK_SET);
  temp = fgetc(inputfile);
  fprintf(outputfile, "%02u", ((temp >> 4) * 10) + (temp & 15));

  fseeko(inputfile, (long long)(0x0017 + offset), SEEK_SET);
  temp = fgetc(inputfile);
  fprintf(outputfile, "%02u.", ((temp >> 4) * 10) + (temp & 15));
  temp = fgetc(inputfile);
  fprintf(outputfile, "%02u.", ((temp >> 4) * 10) + (temp & 15));
  temp = fgetc(inputfile);
  fprintf(outputfile, "%02u", ((temp >> 4) * 10) + (temp & 15));

  fseeko(inputfile, (long long)(0x0026 + offset), SEEK_SET);
  channels = fgetc(inputfile) + 1;
  if(edfplus)
  {
    fprintf(outputfile, "%-8u", (channels + 1) * 256 + 256);
    fprintf(outputfile, "EDF+C");
    for(i=0; i<39; i++)  fputc(' ', outputfile);
  }
  else
  {
    fprintf(outputfile, "%-8u", channels * 256 + 256);
    for(i=0; i<44; i++)  fputc(' ', outputfile);
  }
  fseeko(inputfile, (long long)(0x001c + offset), SEEK_SET);
  if(fread((char *)(&record_duration), 4, 1, inputfile)!=1)
  {
    return(2);
  }
  if((record_duration < 10) || (record_duration > 99999999))
  {
    return(4);
  }
  fprintf(outputfile, "%-8u", record_duration);
  fprintf(outputfile, "0.1     ");
  if(edfplus)  fprintf(outputfile, "%-4u", channels + 1);
  else  fprintf(outputfile, "%-4u", channels);

  for(i=0; i<(channels - 1); i++)
  {
    fseeko(inputfile, (long long)(0x0027 + (i * 10) + offset), SEEK_SET);
    temp = fgetc(inputfile);
    if((temp < 0) || (temp > 255))
    {
      fprintf(outputfile, "%s", "-               ");
    }
    else
    {
      fprintf(outputfile, "%s", labels[temp]);
    }
  }

  fprintf(outputfile, "Events/Markers  ");

  if(edfplus)  fprintf(outputfile, "EDF Annotations ");

  for(i=0; i<(channels * 80); i++)  fputc(' ', outputfile);
  if(edfplus)  for(i=0; i<80; i++)  fputc(' ', outputfile);

  for(i=0; i<(channels - 1); i++)
  {
    fseeko(inputfile, (long long)(0x0027 + (i * 10) + offset), SEEK_SET);
    temp = fgetc(inputfile);
    if(((temp<42)||(temp>73)) && (temp!=76) && (temp!=77))  fprintf(outputfile, "uV      ");
    else  fprintf(outputfile, "mV      ");
  }
  fprintf(outputfile, "        ");
  if(edfplus)  fprintf(outputfile, "        ");

  for(i=0; i<(channels - 1); i++)
  {
    fseeko(inputfile, (long long)(0x0027 + (i * 10) + offset), SEEK_SET);
    temp = fgetc(inputfile);
    if(((temp<42)||(temp>73)) && (temp!=76) && (temp!=77))  fprintf(outputfile, "-3200   ");
    else  fprintf(outputfile, "-12002.9");
  }
  fprintf(outputfile, "-1      ");
  if(edfplus)  fprintf(outputfile, "-1      ");

  for(i=0; i<(channels - 1); i++)
  {
    fseeko(inputfile, (long long)(0x0027 + (i * 10) + offset), SEEK_SET);
    temp = fgetc(inputfile);
    if(((temp<42)||(temp>73)) && (temp!=76) && (temp!=77))  fprintf(outputfile, "3199.902");
    else  fprintf(outputfile, "12002.56");
  }
  fprintf(outputfile, "1       ");
  if(edfplus)  fprintf(outputfile, "1       ");

  for(i=0; i<channels; i++)  fprintf(outputfile, "-32768  ");
  if(edfplus)  fprintf(outputfile, "-32768  ");

  for(i=0; i<channels; i++)  fprintf(outputfile, "32767   ");
  if(edfplus)  fprintf(outputfile, "32767   ");

  for(i=0; i<(channels * 80); i++)  fputc(' ', outputfile);
  if(edfplus)  for(i=0; i<80; i++)  fputc(' ', outputfile);

  fseeko(inputfile, (long long)(0x001b + offset), SEEK_SET);
  samplefrequency = fgetc(inputfile) * 256;
  fseeko(inputfile, (long long)(0x001a + offset), SEEK_SET);
  samplefrequency += fgetc(inputfile);
  samplefrequency &= 0x3fff;
  for(i=0; i<channels; i++)  fprintf(outputfile, "%-8u", samplefrequency / 10);
  if(edfplus)  fprintf(outputfile, "%-8u", ANNOT_TRACKSIZE / 2);

  for(i=0; i<(channels * 32); i++)  fputc(' ', outputfile);
  if(edfplus)  for(i=0; i<32; i++)  fputc(' ', outputfile);

  /************************* write data ****************************************************/

  bufsize = 4194304;
  buf = (char *)calloc(1, bufsize);
  if(buf==NULL)  return(1);

  record_size = (samplefrequency / 10) * channels * 2;
  if(edfplus)  record_size += ANNOT_TRACKSIZE;

  max_buf_records = bufsize / record_size;

  raster = (samplefrequency / 10) * 2;

  seconds = 0;
  deci_seconds = 0;
  n_log_processed = 0;

  fseeko(inputfile, (long long)(0x0027LL + offset + ((channels - 1) * 10LL)), SEEK_SET);

  left_records = record_duration;

  progress_steps = left_records / 100;
  if(progress_steps < 1)
  {
    progress_steps = 1;
  }

  records_written = 0;

  while(left_records)
  {
    if(left_records>max_buf_records)  records_in_buf = max_buf_records;
    else  records_in_buf = left_records;

    for(i=0; i<records_in_buf; i++)
    {
      if(!(records_written%progress_steps))
      {
      }

      for(j=0; j<raster; j+=2)
      {
        for(k=0; k<(channels - 1); k++)
        {
          buf[j+(k*raster)+(i*record_size)] = fgetc(inputfile);
          buf[j+(k*raster)+(i*record_size)+1] = fgetc(inputfile) + 128;
        }
        buf[j+(k*raster)+(i*record_size)] = fgetc(inputfile);
        temp = fgetc(inputfile);
        if(temp==EOF)
        {
          free(buf);
          return(2);
        }
        buf[j+(k*raster)+(i*record_size)+1] = temp;
      }
      if(edfplus)
      {
        annotations = buf + (i * record_size) + (raster * channels);
        memset(annotations, 0, ANNOT_TRACKSIZE);
        p = sprintf(annotations, "%+i.%i", seconds, deci_seconds);
        annotations[p++] = 20;
        annotations[p++] = 20;
        for( ;n_log_processed < n_logs; n_log_processed++)
        {
          elapsed_time = 36000 * (log_buf[(n_log_processed * 45) + 20] - 48);
          elapsed_time += 3600 * (log_buf[(n_log_processed * 45) + 21] - 48);
          elapsed_time += 600 * (log_buf[(n_log_processed * 45) + 22] - 48);
          elapsed_time += 60 * (log_buf[(n_log_processed * 45) + 23] - 48);
          elapsed_time += 10 * (log_buf[(n_log_processed * 45) + 24] - 48);
          elapsed_time += log_buf[(n_log_processed * 45) + 25] - 48;
          if(elapsed_time>=total_elapsed_time)
          {
            elapsed_time -= total_elapsed_time;
            if(elapsed_time<(record_duration / 10))
            {
              p++;
              p += sprintf(annotations + p, "%+i", elapsed_time);
              if(read_subevents)
              {
                annotations[p] = '.';
                p++;
                strncpy(annotations + p, log_buf + (n_log_processed * 45) + 26, 3);
                p += 3;
              }
              annotations[p++] = 20;
              strncpy(annotations + p, log_buf + (n_log_processed * 45), 20);
              p += 20;
              annotations[p] = 20;

              n_log_processed++;

              break;
            }
          }
        }
      }
      if(++deci_seconds>9)
      {
        deci_seconds = 0;
        seconds++;
      }

      records_written++;
    }

    if(fwrite(buf, records_in_buf * record_size, 1, outputfile)!=1)
    {
      free(buf);
      return(3);
    }
    left_records -= records_in_buf;
  }

  total_elapsed_time += record_duration / 10;

  free(buf);

  return(0);
}


int check_device(char *str)
{
  int error = 1;

  if(!strncmp(str, "EEG-1100A V01.00", 16))  error = 0;
  if(!strncmp(str, "EEG-1100B V01.00", 16))  error = 0;
  if(!strncmp(str, "EEG-1100C V01.00", 16))  error = 0;
  if(!strncmp(str, "QI-403A   V01.00", 16))  error = 0;
  if(!strncmp(str, "QI-403A   V02.00", 16))  error = 0;
  if(!strncmp(str, "EEG-2100  V01.00", 16))  error = 0;
  if(!strncmp(str, "EEG-2100  V02.00", 16))  error = 0;
  if(!strncmp(str, "DAE-2100D V01.30", 16))  error = 0;
  if(!strncmp(str, "DAE-2100D V02.00", 16))  error = 0;
  //  if(!strncmp(str, "EEG-1200A V01.00", 16))  error = 0;
  if(!strncmp(str, "EEG-1100A V02.00", 16))  error = 0;
  if(!strncmp(str, "EEG-1100B V02.00", 16))  error = 0;
  if(!strncmp(str, "EEG-1100C V02.00", 16))  error = 0;

  return(error);
}















