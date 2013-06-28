/*
 * Copyright (C) 2005-2008 Patrick Ohly
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <memory>
using namespace std;

#include "config.h"

#ifdef ENABLE_ECAL

#include "EvolutionMemoSource.h"
#include "EvolutionSmartPtr.h"

#include <common/base/Log.h>

SyncItem *EvolutionMemoSource::createItem(const string &luid)
{
    logItem( luid, "extracting from EV" );

    ItemID id(luid);
    eptr<icalcomponent> comp(retrieveItem(id));
    auto_ptr<SyncItem> item(new SyncItem(luid.c_str()));

    item->setData("", 0);
    icalcomponent *cal = icalcomponent_get_first_component(comp, ICAL_VCALENDAR_COMPONENT);
    if (!cal) {
        cal = comp;
    }
    icalcomponent *journal = icalcomponent_get_first_component(cal, ICAL_VJOURNAL_COMPONENT);
    if (!journal) {
        journal = comp;
    }
    icalproperty *summaryprop = icalcomponent_get_first_property(journal, ICAL_SUMMARY_PROPERTY);
    string summary;
    if (summaryprop) {
        const char *summaryptr = icalproperty_get_summary(summaryprop);
        if (summaryptr) {
            summary = summaryptr;
        }
    }
    icalproperty *desc = icalcomponent_get_first_property(journal, ICAL_DESCRIPTION_PROPERTY);
    if (desc) {
        const char *text = icalproperty_get_description(desc);
        if (text) {
            size_t len = strlen(text);
            bool insertSummary = false;
            const char *eol;

            // Check the first line: if it is the same as the summary,
            // then ignore the summary. Otherwise include the summary
            // as first line in the text body. At a receiving Evolution
            // the summary will remain part of the text for compatibility
            // with other clients which might use the first line as part
            // of the normal text.
            eol = strchr(text, '\n');
            if (!eol) {
                eol = text + len;
            }
            if (summary.size() &&
                summary.compare(0, summary.size(), text, eol - text)) {
                insertSummary = true;
            }

            // Replace all \n with \r\n: in the worst case the text
            // becomes twice as long. Also make room for summary.
            eptr<char> dostext((char *)malloc(len * 2 + 1 +
                                              (insertSummary ? summary.size() + 2 : 0)));
            const char *from = text;
            char *to = dostext;
            if (insertSummary) {
                memcpy(to, summary.c_str(), summary.size());
                memcpy(to + summary.size(), "\r\n", 2);
                to += summary.size() + 2;
            }
            eol = strchr(from, '\n');
            while (eol) {
                size_t linelen = eol - from;
                memcpy(to, from, linelen);
                to += linelen;
                from += linelen;
                to[0] = '\r';
                to[1] = '\n';
                to += 2;
                from++;

                eol = strchr(from, '\n');
            }
            memcpy(to, from, strlen(from) + 1);
            item->setData(dostext, strlen(dostext));
        }
    }
    item->setDataType("text/plain");
    item->setModificationTime(0);

    return item.release();
}

EvolutionCalendarSource::InsertItemResult EvolutionMemoSource::insertItem(const string &luid, const SyncItem &item)
{
    const char *type = item.getDataType();

    // fall back to inserting iCalendar 2.0 if
    // real SyncML server has sent vCalendar 1.0 or iCalendar 2.0
    // or the test system inserts such an item
    if (!type[0] ||
        !strcasecmp(type, "raw") ||
        !strcasecmp(type, "text/x-vcalendar") ||
        !strcasecmp(type, "text/calendar")) {
        return EvolutionCalendarSource::insertItem(luid, item);
    }
    
    bool update = !luid.empty();
    bool merged = false;
    string newluid = luid;
    string modTime;

    eptr<char> text;
    text.set((char *)malloc(item.getDataSize() + 1), "copy of item");
    memcpy(text, item.getData(), item.getDataSize());
    text[item.getDataSize()] = 0;

    // replace all \r\n with \n
    char *from = text, *to = text;
    const char *eol = strstr(from, "\r\n");
    while (eol) {
        size_t linelen = eol - from;
        if (to != from) {
            memmove(to, from, linelen);
        }
        to += linelen;
        from += linelen;
        *to = '\n';
        to++;
        from += 2;

        eol = strstr(from, "\r\n");
    }
    if (to != from) {
        memmove(to, from, strlen(from) + 1);
    }

    eol = strchr(text, '\n');
    string summary;
    if (eol) {
        summary.insert(0, (char *)text, eol - (char *)text);
    } else {
        summary = (char *)text;
    }

    eptr<icalcomponent> subcomp(icalcomponent_vanew(
                                    ICAL_VJOURNAL_COMPONENT,
                                    icalproperty_new_summary(summary.c_str()),
                                    icalproperty_new_description(text),
                                    0));

    if( !subcomp ) {
        throwError(string("failure creating vjournal " ) + summary);
    }

    GError *gerror = NULL;
    char *uid = NULL;

    if (!update) {
        if(!e_cal_create_object(m_calendar, subcomp, &uid, &gerror)) {
            if (gerror->domain == E_CALENDAR_ERROR &&
                gerror->code == E_CALENDAR_STATUS_OBJECT_ID_ALREADY_EXISTS) {
                // Deal with error due to adding already existing item.
                // Should never happen for plain text journal entries because
                // they have no embedded ID, but who knows...
                logItem(item, "exists already, updating instead");
                merged = true;
                g_clear_error(&gerror);
            } else {
                throwError( "storing new memo item", gerror );
            }
        } else {
            ItemID id(uid, "");
            newluid = id.getLUID();
            modTime = getItemModTime(id);
        }
    }

    if (update || merged) {
        ItemID id(newluid);

        // ensure that the component has the right UID
        if (update && !id.m_uid.empty()) {
            icalcomponent_set_uid(subcomp, id.m_uid.c_str());
        }

        if (!e_cal_modify_object(m_calendar, subcomp, CALOBJ_MOD_ALL, &gerror)) {
            throwError(string("updating memo item ") + item.getKey(), gerror);
        }
        ItemID newid = getItemID(subcomp);
        newluid = newid.getLUID();
        modTime = getItemModTime(newid);
    }

    return InsertItemResult(newluid, modTime, merged);
}

#endif /* ENABLE_ECAL */
