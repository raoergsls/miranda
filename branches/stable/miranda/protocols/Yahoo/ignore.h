/*
 * $Id$
 *
 * myYahoo Miranda Plugin 
 *
 * Authors: Gennady Feldman (aka Gena01) 
 *          Laurent Marechal (aka Peorth)
 *
 * This code is under GPL and is based on AIM, MSN and Miranda source code.
 * I want to thank Robert Rainwater and George Hazan for their code and support
 * and for answering some of my questions during development of this plugin.
 */
#ifndef _YAHOO_IGNORE_H_
#define _YAHOO_IGNORE_H_

int YAHOO_BuddyIgnored(const char *who);
const YList* YAHOO_GetIgnoreList(void);
void YAHOO_IgnoreBuddy(const char *buddy, int ignore);
void ext_yahoo_got_ignore(int id, YList * igns);

#endif
