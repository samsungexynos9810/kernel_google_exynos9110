/*
*  Definitions of the RF module for Kingyo board
*
*  Copyright (C) 2017 CASIO Computer Co.,Ltd.
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*/

#define POWER_ON    1
#define POWER_OFF   0

#define BT_RESET_GPIO		81	// BT_REG_ON
#define BT_HOST_WAKE_GPIO	18	// BT->HOST wake
#define BT_WAKE_GPIO		80	// HOST->BT wake
#define BT_HOST_TX_GPIO	31	// HOST->BT TX
#define BT_HOST_RTS_GPIO	32	// HOST->BT RTS
#define WLAN_RESET_GPIO	84	// WL_REG_ON
#define WLAN_HOST_WAKE_GPIO	16	// WLAN->HOST wake
