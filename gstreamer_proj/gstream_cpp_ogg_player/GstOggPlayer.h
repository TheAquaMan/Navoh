/*
 * GstOggPlayer.h
 *
 *  Created on: 13.10.2011
 *      Author: babah
 */

#ifndef GSTOGGPLAYER_H_
#define GSTOGGPLAYER_H_

#include "GstPlayerBase.h"
#include <string>
#include <iostream>

using namespace std;

class GstOggPlayer : public GstPlayerBase
{
	std::string m_Filename;
	Glib::RefPtr<Gst::Element> source, parser, decoder, conv, sink;

public:
	void Init();
	void SetFile(const std::string& fileName);
	GstOggPlayer();
	GstOggPlayer(const std::string& fileName);
	~GstOggPlayer();
	gint64 get_duration()
	{
		gint64 duration;
		Gst::Format fmt = Gst::FORMAT_TIME;
		pipeline->query_duration(fmt, duration);
		return duration;
	}
	Gst::State getState()
	{
		Gst::State state;
		Gst::State pending;
		Gst::StateChangeReturn ret;

		ret = pipeline->get_state(state, pending, Gst::SECOND * 1);

		if (ret == Gst::STATE_CHANGE_SUCCESS)
			return state;

		return Gst::STATE_NULL;
	}
	bool isPlaying()
	{
		return getState() == Gst::STATE_PLAYING;
	}

	bool on_bus_Message(const Glib::RefPtr<Gst::Bus>& bus, const Glib::RefPtr<Gst::Message>& message)
	{
		switch (message->get_message_type())
		{
		case Gst::MESSAGE_EOS:
			Stop();
			break;
		case Gst::MESSAGE_ERROR:
			{
				Glib::RefPtr<Gst::MessageError> msgError = Glib::RefPtr<Gst::MessageError>::cast_dynamic(message);
				if(msgError)
				{
					Glib::Error err;
					err = msgError->parse();
					std::cerr << "Error: " << err.what() << std::endl;
				}
				else
					std::cerr << "Error." << std::endl;

			Stop();
			return false;
			}
		default:
			break;
		}
		return true;
	}

private:
	void on_parser_pad_added(const Glib::RefPtr<Gst::Pad>& newPad);
};


#endif /* GSTOGGPLAYER_H_ */
