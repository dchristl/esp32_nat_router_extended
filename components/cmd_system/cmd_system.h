/* Console example — various system commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef CMD_SYSTEM_H
#define CMD_SYSTEM_H

   extern const char *GLOBAL_VERSION;
   extern const char *GLOBAL_HASH;
   extern const char *GLOBAL_BUILD_DATE;
#endif

   // Register system functions
   void register_system(void);
   void determineChipType(char chip_type[30]);

#ifdef __cplusplus
}
#endif
