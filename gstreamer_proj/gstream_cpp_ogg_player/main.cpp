/*
 * main.cpp
 *
 *  Created on: 20.10.2011
 *      Author: babah
 */

#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <libxml++/libxml++.h>
#include "GstOggPlayer.h"
#include <gstreamermm.h>

using namespace std;

struct DetectedBarcode
{
	string m_Message;
	string m_Direction;
	double m_Distance;
public:
	DetectedBarcode(){}
	DetectedBarcode(string inMessage, string inDirection, double inDistance) :
		m_Message(inMessage), m_Direction(inDirection), m_Distance(inDistance) {}
	void toXML(xmlpp::Element* root);
	void fromXML(xmlpp::Element* root);

	friend ostream& operator<<(ostream& o, DetectedBarcode& inObj)
	{
		o << "Message: " << inObj.m_Message
				<< "\nDirection: " << inObj.m_Direction
				<< "\nDistance: " << inObj.m_Distance << endl;
		return o;
	}
};


void DetectedBarcode::toXML(xmlpp::Element* root)
{
	using namespace xmlpp;

	Element* element = root->add_child("Object");
	//element->set_attribute("attribute", "value");

	Element* message = element->add_child("message");
	message->add_child_text(m_Message);

	Element* direction = element->add_child("direction");
	direction->add_child_text(m_Direction);

	Element* distance = element->add_child("distance");
	distance->add_child_text(boost::lexical_cast<string>(m_Distance));
}

void DetectedBarcode::fromXML(xmlpp::Element* root)
{
	using namespace xmlpp;

	Element* message = dynamic_cast<Element*>(root->get_children("message").front());
	m_Message = message->get_child_text()->get_content();

	Element* direction = dynamic_cast<Element*>(root->get_children("direction").front());
	m_Direction = direction->get_child_text()->get_content();

	Element* distance = dynamic_cast<Element*>(root->get_children("distance").front());
	m_Distance = boost::lexical_cast<double>(distance->get_child_text()->get_content());
}

void listDetectedBarcodesToXML(xmlpp::Element* root, vector<DetectedBarcode>& list)
{
	for (vector<DetectedBarcode>::iterator it = list.begin();
			it != list.end();
			++it)
	{
		it->toXML(root);
	}
}

void listDetectedBarcodesFromXML(xmlpp::Element* root, vector<DetectedBarcode>& list)
{
	using namespace xmlpp;


	if (root->get_name() != "ObjectList")
	{
		cerr << "Bad document content type: ObjectList expected" << endl;
		return;
	}

	Node::NodeList element_list = root->get_children("Object");

	for (Node::NodeList::iterator it = element_list.begin();
			it != element_list.end();
			++it)
	{
		DetectedBarcode obj;
		obj.fromXML(dynamic_cast<Element*>(*it));
		list.push_back(obj);
	}
}


using boost::asio::ip::tcp;

void network_activity()
{
	try
		{
			for (;;)
			{

				boost::asio::io_service io_service;

				tcp::resolver resolver(io_service);
				tcp::resolver::query query("127.0.0.1", "5001");
				tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
				tcp::resolver::iterator end;

				tcp::socket socket(io_service);

				boost::system::error_code error = boost::asio::error::host_not_found;
				while (error && endpoint_iterator != end)
				{
					socket.close();
					socket.connect(*endpoint_iterator++, error);
				}
				if (error)
					throw boost::system::system_error(error);

				string str;
				for (;;)
				{
					boost::array<char, 128> buf;
					boost::system::error_code error;
					size_t len = socket.read_some(boost::asio::buffer(buf), error);
					if (error == boost::asio::error::eof)
						break; // Connection closed cleanly by peer.
					else if (error)
						throw boost::system::system_error(error); // Some other error.

					str.append(buf.begin(), buf.begin() + len);
				}

				if (!str.empty())
				{
					cout << str << endl;
					xmlpp::DomParser parser;
					parser.parse_memory(str);
					xmlpp::Document* document2 = parser.get_document();
					vector<DetectedBarcode> vec;

					listDetectedBarcodesFromXML(document2->get_root_node(), vec);

					for (vector<DetectedBarcode>::iterator it = vec.begin();
							it != vec.end();
							++it)
					{
						GstOggPlayer player("./version_1/"  + it->m_Direction + ".ogg");
						player.Init();
						player.Play();
						while (player.isPlaying()) sleep(1);

						string place;
						switch (boost::lexical_cast<int>(it->m_Message))
						{
						case 1:
							place = "laboratory";
							break;
						case 2:
							place = "exit";
							break;
						case 3:
							place = "meeting";
							break;
						case 4:
							place = "compclass";
							break;
						}
						player.SetFile("./version_1/" + place + ".ogg");
						player.Init();
						player.Play();
						while (player.isPlaying()) sleep(1);

						double tmp = double((int)it->m_Distance) + (double(it->m_Distance - (int)it->m_Distance) < 0.6  ? 0.5 : 1);
						string filename;
						if (tmp > 10)
							filename = "more10";
						else
							filename = boost::lexical_cast<string>(tmp);

						player.SetFile("./version_1/" + filename + "meter" + ".ogg");
						player.Init();
						player.Play();
						while (player.isPlaying()) sleep(1);
					}
				}
			}
		}
		catch (std::exception& e)
		{
			std::cerr << e.what() << std::endl;
		}
}


int main(int argc, char **argv)
{
	Gst::init();

	boost::thread thread(&network_activity);

	Glib::RefPtr<Glib::MainLoop> mainloop;
	mainloop = Glib::MainLoop::create();
	mainloop->run();


	return 0;
}
