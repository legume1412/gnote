/*
 * gnote
 *
 * Copyright (C) 2009 Hubert Figuiere
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
 */



#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include <glibmm/i18n.h>


#include "debug.hpp"
#include "notebuffer.hpp"
#include "notewindow.hpp"

#include "bugzillanoteaddin.hpp"
#include "bugzillalink.hpp"
#include "bugzillapreferencesfactory.hpp"
#include "insertbugaction.hpp"

namespace bugzilla {

  BugzillaModule::BugzillaModule()
  {
    ADD_INTERFACE_IMPL(BugzillaNoteAddin);
    ADD_INTERFACE_IMPL(BugzillaPreferencesFactory);
  }
  const char * BugzillaModule::id() const
  {
    return "BugzillaAddin";
  }
  const char * BugzillaModule::name() const
  {
    return _("Bugzilla Links");
  }
  const char * BugzillaModule::description() const
  {
    return _("Allows you to drag a Bugzilla URL from your browser directly into a Gnote note.  The bug number is inserted as a link with a little bug icon next to it.");
  }
  const char * BugzillaModule::authors() const
  {
    return _("Hubert Figuiere and the Tomboy Project");
  }
  const char * BugzillaModule::category() const
  {
    return "Desktop Integration";
  }
  const char * BugzillaModule::version() const
  {
    return "0.1";
  }


  const char * BugzillaNoteAddin::TAG_NAME = "link:bugzilla";

  void BugzillaNoteAddin::initialize()
  {
    if(!get_note()->get_tag_table()->is_dynamic_tag_registered(TAG_NAME)) {
      get_note()->get_tag_table()
        ->register_dynamic_tag(TAG_NAME, sigc::ptr_fun(&BugzillaLink::create));
    }
  }



  void BugzillaNoteAddin::shutdown()
  {
  }


  void BugzillaNoteAddin::on_note_opened()
  {
    get_window()->editor()->signal_drag_data_received().connect(
      sigc::mem_fun(*this, &BugzillaNoteAddin::on_drag_data_received), false);
  }


  void BugzillaNoteAddin::on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, 
                                                int x, int y, 
                                                const Gtk::SelectionData & selection_data, 
                                                guint, guint time)
  {
    DBG_OUT("Bugzilla.OnDragDataReceived");
    Gdk::ListHandle_AtomString targets = context->get_targets();

    for(Gdk::ListHandle_AtomString::const_iterator iter = targets.begin();
        iter != targets.end(); ++iter) {
      
      std::string atom(*iter);
      DBG_OUT("atom is %s", atom.c_str());
      if((atom == "text/uri-list") || (atom == "_NETSCAPE_URL")) {
        drop_uri_list(context, x, y, selection_data, time);
        return;
      }
    }
  }


  void BugzillaNoteAddin::drop_uri_list(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, 
                                        const Gtk::SelectionData & selection_data, guint time)
  {
    std::string uriString = selection_data.get_text();
    if(uriString.empty()) {
      return;
    }

    const char * regexString = "\\bhttps?://.*/show_bug\\.cgi\\?(\\S+\\&){0,1}id=(\\d{1,})";

    boost::regex re(regexString, boost::regex::extended|boost::regex_constants::icase);
    boost::match_results<std::string::const_iterator> m;
    if(regex_match(uriString, m, re) && m[2].matched) {
      try {
        int bugId = boost::lexical_cast<int>(std::string(m[2].first, m[2].second));

        if (insert_bug (x, y, uriString, bugId)) {
          context->drag_finish(true, false, time);
          g_signal_stop_emission_by_name(get_window()->editor()->gobj(),
                                         "drag_data_received");
        }
      }
      catch(const std::exception & e) {
        ERR_OUT("exception while converting URL '%s': %s",
                uriString.c_str(), e.what());
      }
    }
  }


  bool BugzillaNoteAddin::insert_bug(int x, int y, const std::string & uri, int id)
  {
    try {
      BugzillaLink::Ptr link_tag = 
        BugzillaLink::Ptr::cast_dynamic(get_note()->get_tag_table()->create_dynamic_tag(TAG_NAME));
      link_tag->set_bug_url(uri);

      // Place the cursor in the position where the uri was
      // dropped, adjusting x,y by the TextView's VisibleRect.
      Gdk::Rectangle rect;
      get_window()->editor()->get_visible_rect(rect);
      x = x + rect.get_x();
      y = y + rect.get_y();
      Gtk::TextIter cursor;
      gnote::NoteBuffer::Ptr buffer = get_buffer();
      get_window()->editor()->get_iter_at_location(cursor, x, y);
      buffer->place_cursor (cursor);

      std::string string_id = boost::lexical_cast<std::string>(id);
      buffer->undoer().add_undo_action (new InsertBugAction (cursor, 
                                                             string_id, 
                                                             buffer,
                                                             link_tag));

      std::vector<Glib::RefPtr<Gtk::TextTag> > tags;
      tags.push_back(link_tag);
      buffer->insert_with_tags (cursor, 
                                string_id, 
                                tags);
      return true;
    } 
    catch (...)
    {
		}
    return false;
  }

}
