/*******************************************************************************/
/*                                                                             */
/* This program is free software; you can redistribute it and/or modify it     */
/* under the terms of the GNU General Public License as published by the       */
/* Free Software Foundation; either version 2 of the License, or (at your      */
/* option) any later version.                                                  */
/*                                                                             */
/* This program is distributed in the hope that it will be useful, but WITHOUT */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   */
/* more details.                                                               */
/*                                                                             */
/* You should have received a copy of the GNU General Public License along     */
/* with This program; see the file COPYING.  If not,write to the Free Software */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */
/*******************************************************************************/

/* 
 * File:   VST3PluginHost.h
 * Author: sspresto
 *
 * Created on December 29, 2023, 1:36 PM
 */

#pragma once

#ifdef VST3_SUPPORT

#include <unordered_map>
#include <locale>
#include <string>
#include <vector>
#include <list>
#include <codecvt>
#include <jack/types.h>

#include "pluginterfaces/vst/ivstpluginterfacesupport.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivsthostapplication.h"

//#define ASRT_TMP ASSERT
#undef ASSERT       // Fix redefinition with /nonlib/debug.h"
#undef WARNING      // Fix redefinition with /nonlib/debug.h"
#include "base/source/fobject.h"
#undef ASSERT       // Fix redefinition with /nonlib/debug.h"
#undef WARNING      // Fix redefinition with /nonlib/debug.h"

#include "../../../nonlib/debug.h"

#define WARNING( fmt, args... ) warnf( W_WARNING, __MODULE__, __FILE__, __FUNCTION__, __LINE__, fmt, ## args )

std::string utf16_to_utf8(const std::u16string& utf16);

using namespace Steinberg;
using namespace Linux;

class VST3_Plugin;

class VST3PluginHost : public Vst::IHostApplication
{
public:

    // Constructor.
    VST3PluginHost (VST3_Plugin * plug);

    // Destructor.
    virtual ~VST3PluginHost ();

    DECLARE_FUNKNOWN_METHODS

    //--- IHostApplication ---
    //
    tresult PLUGIN_API getName (Vst::String128 name) override;
    tresult PLUGIN_API createInstance (TUID cid, TUID _iid, void **obj) override;

    FUnknown *get() { return static_cast<Vst::IHostApplication *> (this); }

    // Timer stuff...
    //
    void startTimer (int msecs);
    void stopTimer ();

    int timerInterval() const;

    // RunLoop adapters...
    //
    tresult registerEventHandler (IEventHandler *handler, FileDescriptor fd);
    tresult unregisterEventHandler (IEventHandler *handler);

    tresult registerTimer (ITimerHandler *handler, TimerInterval msecs);
    tresult unregisterTimer (ITimerHandler *handler);

    // Executive methods.
    //
    void processTimers();
    void processEventHandlers();

#ifdef CONFIG_VST3_XCB
    void openXcbConnection();
    void closeXcbConnection();
#endif

    // Common host time-keeper context accessors.
    Vst::ProcessContext *processContext();

    void processAddRef();
    void processReleaseRef();

    // Common host time-keeper process context.
    void updateProcessContext(jack_position_t &pos, const bool &xport_changed, const bool &has_bbt);

    // Cleanup.
    void clear();

protected:

    class PlugInterfaceSupport;

    class Attribute;
    class AttributeList;
    class Message;

    class Timer;

private:

    // Instance members.
    IPtr<PlugInterfaceSupport> m_plugInterfaceSupport;

    VST3_Plugin * m_plugin;

    Timer *m_pTimer;

    unsigned int m_timerRefCount;

    struct TimerHandlerItem
    {
        TimerHandlerItem(ITimerHandler *h, TimerInterval i)
                : handler(h), interval(i), counter(0) {}

        void reset(TimerInterval i)
            { interval = i; counter = 0; }

        ITimerHandler *handler;
        TimerInterval  interval;
        TimerInterval  counter;
    };

    std::unordered_map<ITimerHandler *, TimerHandlerItem *> m_timerHandlers;
    //QHash<ITimerHandler *, TimerHandlerItem *> m_timerHandlers;

    std::list<TimerHandlerItem *> m_timerHandlerItems;
    //QList<TimerHandlerItem *> m_timerHandlerItems;

    std::unordered_map<IEventHandler *, int> m_eventHandlers;
    //QMultiHash<IEventHandler *, int> m_eventHandlers;

#ifdef CONFIG_VST3_XCB
    xcb_connection_t *m_pXcbConnection;
    int               m_iXcbFileDescriptor;
#endif

    Vst::ProcessContext m_processContext;
    unsigned int        m_processRefCount;
};


//-----------------------------------------------------------------------------
//
class VST3PluginHost::PlugInterfaceSupport
	: public FObject, public Vst::IPlugInterfaceSupport
{
public:

    // Constructor.
    PlugInterfaceSupport ()
    {
        addPluInterfaceSupported(Vst::IComponent::iid);
        addPluInterfaceSupported(Vst::IAudioProcessor::iid);
        addPluInterfaceSupported(Vst::IEditController::iid);
        addPluInterfaceSupported(Vst::IConnectionPoint::iid);
        addPluInterfaceSupported(Vst::IUnitInfo::iid);
//	addPluInterfaceSupported(Vst::IUnitData::iid);
        addPluInterfaceSupported(Vst::IProgramListData::iid);
        addPluInterfaceSupported(Vst::IMidiMapping::iid);
//	addPluInterfaceSupported(Vst::IEditController2::iid);
    }

    OBJ_METHODS (PlugInterfaceSupport, FObject)
    REFCOUNT_METHODS (FObject)
    DEFINE_INTERFACES
        DEF_INTERFACE (Vst::IPlugInterfaceSupport)
    END_DEFINE_INTERFACES (FObject)

    //--- IPlugInterfaceSupport ----
    //
    tresult PLUGIN_API isPlugInterfaceSupported (const TUID _iid) override
    {
        //if (m_fuids.contains(QString::fromLocal8Bit(_iid)))
        for( unsigned i = 0; i < m_fuids.size(); ++i)
        {
            if ( strcmp(_iid, m_fuids[i].c_str() ) == 0)
                return kResultOk;
        }
    //	else
        return kResultFalse;
    }

protected:

    void addPluInterfaceSupported(const TUID& _iid)
        { m_fuids.push_back(_iid); }
    //	{ m_fuids.append(QString::fromLocal8Bit(_iid)); }

private:

    // Instance members.
    std::vector<std::string> m_fuids;
};


//-----------------------------------------------------------------------------
//
class VST3PluginHost::Attribute
{
public:

    enum Type
    {
        kInteger,
        kFloat,
        kString,
        kBinary
    };

    // Constructors.
    Attribute (int64 value) : m_size(0), m_type(kInteger)
        { m_v.intValue = value; }

    Attribute (double value) : m_size(0), m_type(kFloat)
        { m_v.floatValue = value; }

    Attribute (const Vst::TChar *value, uint32 size)
            : m_size(size), m_type(kString)
    {
        m_v.stringValue = new Vst::TChar[size];
        ::memcpy(m_v.stringValue, value, size * sizeof (Vst::TChar));
    }

    Attribute (const void *value, uint32 size)
            : m_size(size), m_type(kBinary)
    {
        m_v.binaryValue = new char[size];
        ::memcpy(m_v.binaryValue, value, size);
    }

    // Destructor.
    ~Attribute ()
    {
        if (m_size)
            delete [] m_v.binaryValue;
    }

    // Accessors.
    int64 intValue () const
        { return m_v.intValue; }

    double floatValue () const
        { return m_v.floatValue; }

    const Vst::TChar *stringValue ( uint32& stringSize )
    {
        stringSize = m_size;
        return m_v.stringValue;
    }

    const void *binaryValue ( uint32& binarySize )
    {
        binarySize = m_size;
        return m_v.binaryValue;
    }

    Type getType () const
        { return m_type; }

protected:

    // Instance members.
    union v
    {
        int64  intValue;
        double floatValue;
        Vst::TChar *stringValue;
        char  *binaryValue;

    } m_v;

    uint32 m_size;
    Type m_type;
};


//-----------------------------------------------------------------------------
//
class VST3PluginHost::AttributeList : public Vst::IAttributeList
{
public:

    // Constructor.
    AttributeList ()
    {
        FUNKNOWN_CTOR
    }

    // Destructor.
    virtual ~AttributeList ()
    {
        for (auto i : m_list)
        {
            delete i.second;
        }
    //	qDeleteAll(m_list);
        m_list.clear();

        FUNKNOWN_DTOR
    }

    DECLARE_FUNKNOWN_METHODS

    //--- IAttributeList ---
    //
    tresult PLUGIN_API setInt (AttrID aid, int64 value) override
    {   
        removeAttrID(aid);

        std::pair<std::string, Attribute *> Attr ( aid, new Attribute(value) );
        m_list.insert(Attr);
        //	m_list.insert(aid, new Attribute(value));
        return kResultTrue;
    }

    tresult PLUGIN_API getInt (AttrID aid, int64& value) override
    {
        std::unordered_map<std::string, Attribute *>::const_iterator got
            = m_list.find (aid);

        if ( got == m_list.end() )
        {
            return kResultFalse;
        }

        Attribute *attr = got->second;
    //	Attribute *attr = m_list.value(aid, nullptr);
        if (attr)
        {
            value = attr->intValue();
            return kResultTrue;
        }

        return kResultFalse;
    }

    tresult PLUGIN_API setFloat (AttrID aid, double value) override
    {
        removeAttrID(aid);

        std::pair<std::string, Attribute *> Attr ( aid, new Attribute(value) );
        m_list.insert(Attr);

      //  m_list.insert(aid, new Attribute(value));
        return kResultTrue;
    }

    tresult PLUGIN_API getFloat (AttrID aid, double& value) override
    {
        std::unordered_map<std::string, Attribute *>::const_iterator got
            = m_list.find (aid);

        if ( got == m_list.end() )
        {
            return kResultFalse;
        }

        Attribute *attr = got->second;

     //   Attribute *attr = m_list.value(aid, nullptr);
        if (attr)
        {
            value = attr->floatValue();
            return kResultTrue;
        }

        return kResultFalse;
    }

    tresult PLUGIN_API setString (AttrID aid, const Vst::TChar *string) override
    {
        removeAttrID(aid);

        std::pair<std::string, Attribute *> prm ( aid, new Attribute( string, utf16_to_utf8(string).length() ) );
            m_list.insert(prm);

        return kResultTrue;
#if 0
        removeAttrID(aid);
        m_list.insert(aid, new Attribute(string, fromTChar(string).length()));
        return kResultTrue;
#endif

    }

    tresult PLUGIN_API getString (AttrID aid, Vst::TChar *string, uint32 size) override
    {
        std::unordered_map<std::string, Attribute *>::const_iterator got
            = m_list.find (aid);

        if ( got == m_list.end() )
        {
            return kResultFalse;
        }

        Attribute *attr = got->second;
        if (attr)
        {
            uint32 size2 = 0;
            const Vst::TChar *string2 = attr->stringValue(size2);
            ::memcpy(string, string2, (size < size2 ? size: size2) * sizeof(Vst::TChar));
            return kResultTrue;
        }
        return kResultFalse;
#if 0
        Attribute *attr = m_list.value(aid, nullptr);
        if (attr) {
                uint32 size2 = 0;
                const Vst::TChar *string2 = attr->stringValue(size2);
                ::memcpy(string, string2, qMin(size, size2) * sizeof(Vst::TChar));
                return kResultTrue;
        }
#endif
    }

    tresult PLUGIN_API setBinary (AttrID aid, const void* data, uint32 size) override
    {
        removeAttrID(aid);

        std::pair<std::string, Attribute *> Attr ( aid, new Attribute(data, size) );
        m_list.insert(Attr);

      //  m_list.insert(aid, new Attribute(data, size));
        return kResultTrue;
    }

    tresult PLUGIN_API getBinary (AttrID aid, const void*& data, uint32& size) override
    {
        std::unordered_map<std::string, Attribute *>::const_iterator got
            = m_list.find (aid);

        if ( got == m_list.end() )
        {
            return kResultFalse;
        }

        Attribute *attr = got->second;
       // Attribute *attr = m_list.value(aid, nullptr);
        if (attr)
        {
            data = attr->binaryValue(size);
            return kResultTrue;
        }
        size = 0;
        return kResultFalse;
    }

protected:

    void removeAttrID (AttrID aid)
    {
        std::unordered_map<std::string, Attribute *>::const_iterator got
            = m_list.find (aid);

        if ( got == m_list.end() )
        {
            return;
        }

        Attribute *attr = got->second;
      //  Attribute *attr = m_list.value(aid, nullptr);
        if (attr)
        {
            delete attr;
            m_list.erase(aid);
          //  m_list.remove(aid);
        }
    }

private:

    // Instance members.
    std::unordered_map<std::string, Attribute *> m_list;
    //QHash<QString, Attribute *> m_list;
};

//IMPLEMENT_FUNKNOWN_METHODS (VST3PluginHost::AttributeList, IAttributeList, IAttributeList::iid)


//-----------------------------------------------------------------------------
//
class VST3PluginHost::Message : public Vst::IMessage
{
public:

    // Constructor.
    Message () : m_messageId(nullptr), m_attributeList(nullptr)
    {
        FUNKNOWN_CTOR
    }

    // Destructor.
    virtual ~Message ()
    {
        setMessageID(nullptr);

        if (m_attributeList)
            m_attributeList->release();

        FUNKNOWN_DTOR
    }

    DECLARE_FUNKNOWN_METHODS

    //--- IMessage ---
    //
    const char *PLUGIN_API getMessageID () override
        { return m_messageId; }

    void PLUGIN_API setMessageID (const char *messageId) override
    {
        if (m_messageId)
            delete [] m_messageId;

        m_messageId = nullptr;

        if (messageId)
        {
            size_t len = strlen(messageId) + 1;
            m_messageId = new char[len];
            ::strcpy(m_messageId, messageId);
        }
    }

    Vst::IAttributeList* PLUGIN_API getAttributes () override
    {
        if (!m_attributeList)
            m_attributeList = new AttributeList();

        return m_attributeList;
    }

protected:

    // Instance members.
    char *m_messageId;

    AttributeList *m_attributeList;
};

//IMPLEMENT_FUNKNOWN_METHODS (VST3PluginHost::Message, IMessage, IMessage::iid)


//-----------------------------------------------------------------------------
// class VST3PluginHost::Timer -- VST3 plugin host timer impl.
//

class VST3PluginHost::Timer
{
public:

    // Constructor.
    Timer (VST3PluginHost *pHost) : m_pHost(pHost) {}

    // Main method.
    void start (int msecs)
    {
//        f_miliseconds = float(msecs) *.001;

    //    DMESSAGE("Miliseconds = %f", f_miliseconds);

    //    Fl::add_timeout( f_miliseconds, &VST3_Plugin::custom_update_ui, g_VST3_Plugin );

#if 0
        int iInterval = QTimer::interval();
        if (iInterval == 0)
                iInterval = DEFAULT_MSECS;
        if (iInterval > msecs)
                iInterval = msecs;

        QTimer::start(iInterval);
#endif
    }

    void stop()
    {
       // Fl::remove_timeout(&VST3_Plugin::custom_update_ui, g_VST3_Plugin);
    }

    int interval()
    {
       // DMESSAGE("Interval = %d", int(f_miliseconds * 1000) );
//        return int(f_miliseconds * 1000);
        return 33;
    }

protected:

#if 0
    void timerEvent (QTimerEvent *pTimerEvent)
    {
        if (pTimerEvent->timerId() == QTimer::timerId()) {
                m_pHost->processTimers();
                m_pHost->processEventHandlers();
        }
    }
#endif

private:

    // Instance members.
    VST3PluginHost *m_pHost;
};

#endif  // VST3_SUPPORT