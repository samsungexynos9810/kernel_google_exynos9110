/*
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

static struct semaphore bcmlock;

/* Power lock waiting timeout, in second */
#define BCMLOCK_TIMEOUT 2
static int count = 1;
static int owner_cookie = -1;

#define PR(msg, ...) printk("BCM## "msg, ##__VA_ARGS__)

int bcm_power_lock(int cookie)
{
	int ret;
	char cookie_msg[5] = {0};
	ret = down_timeout(&bcmlock, msecs_to_jiffies(BCMLOCK_TIMEOUT*1000));
	if (ret == 0) {
		memcpy(cookie_msg, &cookie, sizeof(cookie));
		owner_cookie = cookie;
		count--;
		PR("bcm powerlock acquired cookie: %s\n", cookie_msg);
	} else {
		PR("warn: bcm powerlock failed to acquire cookie: %s\n", cookie_msg);
	}
	return ret;
}
EXPORT_SYMBOL(bcm_power_lock);

void bcm_power_unlock(int cookie)
{
	char owner_msg[5] = {0};
	char cookie_msg[5] = {0};
	memcpy(cookie_msg, &cookie, sizeof(cookie));
	if (owner_cookie == cookie) {
		owner_cookie = -1;
		if (count++ > 1)
			PR("error, release a lock that was not acquired\n");
		up(&bcmlock);
		PR("bcm powerlock released, cookie: %s\n", cookie_msg);
	} else {
		memcpy(owner_msg, &owner_cookie, sizeof(owner_cookie));
		PR("ignore lock release, cookie mismatch: %s owner %s\n", 
			cookie_msg, owner_cookie == 0 ? "NULL" : owner_msg);
	}
}
EXPORT_SYMBOL(bcm_power_unlock);

void bcm_powerlock_init(void) {
	sema_init(&bcmlock, 1);
}
EXPORT_SYMBOL(bcm_powerlock_init);


