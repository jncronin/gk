/*******************************************************************************
 * File Name: cycfg_system.h
 *
 * Description:
 * System configuration
 * This file was automatically generated and should not be modified.
 * Tools Package 2.4.0.5972
 * mtb-pdl-cat1 2.4.1.17937
 * personalities 6.0.0.0
 * udd 3.0.0.2024
 *
 ********************************************************************************  *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ********************************************************************************/

#if !defined(CYCFG_SYSTEM_H)
#define CYCFG_SYSTEM_H

#if defined(__cplusplus)
extern "C" {
#endif

#define CY_CFG_PWR_MODE_LP 0x01UL
#define CY_CFG_PWR_MODE_ULP 0x02UL
#define CY_CFG_PWR_MODE_ACTIVE 0x04UL
#define CY_CFG_PWR_MODE_SLEEP 0x08UL
#define CY_CFG_PWR_MODE_DEEPSLEEP 0x10UL
#define CY_CFG_PWR_SYS_IDLE_MODE CY_CFG_PWR_MODE_SLEEP
#define CY_CFG_PWR_SYS_ACTIVE_MODE CY_CFG_PWR_MODE_LP
#define CY_CFG_PWR_DEEPSLEEP_LATENCY 0UL

#if defined(__cplusplus)
}
#endif


#endif /* CYCFG_SYSTEM_H */

