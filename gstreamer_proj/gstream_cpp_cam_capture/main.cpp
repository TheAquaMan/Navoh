
/*
 * main.cpp
 *
 *  Created on: 08.10.2011
 *      Author: babah
 */

#include <gtkmm/window.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/main.h>
#include <gtkmm/drawingarea.h>
#include <gdk/gdkpixbuf.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/box.h>
#include <pangomm.h>
#include <libxml++/libxml++.h>
#include <cairomm/cairomm.h>
#include <gstreamermm.h>
#include "GstVideoClient.h"
#include <dmtx.h>
#include <iostream>
#include <vector>
#include <string>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "math.h"
using namespace std;
using namespace boost;

boost::mutex mut;

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

class BarcodePropertyEstimation
{
	int ImageWidth;
	int ImageHeight;					//		B-----------C
	double Ax, Ay, Bx, By;				//		|	        |
	double Cx, Cy, Dx, Dy;				//      A---------- D

public:
	BarcodePropertyEstimation(int width, int height,
			double inAx, double inAy,
			double inBx, double inBy,
			double inCx, double inCy,
			double inDx, double inDy) :
			ImageWidth(width), ImageHeight(height),
			Ax(inAx), Ay(inAy), Bx(inBx), By(inBy),
			Cx(inCx), Cy(inCy), Dx(inDx), Dy(inDy){}

	double calcDistance()  // approximetely dist to Mtx code
	{
		double div, dst;
		div = ImageHeight * ImageWidth / calcSquare();
		dst = sqrt(div / 7.9);
		return dst;
	}
	double calcDirection()	// returns from -10 (left) to +10 right
	{
		double midX = (Ax + Cx) / 2;
		double direction = (midX - ImageWidth / 2) / (ImageWidth / 2) * 10;
		return direction;
	}
	double calcSquare()
	{
		double d1, d2, d;
		double square;
		d1 = sqrt(pow(Ax - Cx, 2) + pow(Ay - Cy, 2));
		d2 = sqrt(pow(Bx - Dx, 2) + pow(By - Dy, 2));
		d = max(d1, d2);
		square = pow(d, 2) / 2;
		return square;
	}
};
using boost::asio::ip::tcp;


void send_to_client( string& str)
{
	try
	{
		boost::asio::io_service io_service;

		tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 5001));

		for (;;)
		{
			tcp::socket socket(io_service);

			mut.lock();
			str.clear();
			mut.unlock();

			acceptor.accept(socket);

			boost::thread::yield();

			mut.lock();
			boost::asio::write(socket, boost::asio::buffer(str));
			mut.unlock();
		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}

class VideoWidget : public Gtk::DrawingArea
{
	GstVideoClient vc;
	Glib::RefPtr<Gdk::Pixbuf> pixbuf;
public:
	string strXML;


	VideoWidget()
	{
		vc.Init();
		vc.Play();
		vc.get_frame(pixbuf);
		set_size_request(pixbuf->get_width(), pixbuf->get_height());
	}

	virtual ~VideoWidget(){}

	virtual bool on_expose_event(GdkEventExpose* )
	{
		Glib::RefPtr<Gdk::Drawable> drawable = get_window();

		vc.get_frame(pixbuf);

		drawable->draw_pixbuf(get_style()->get_bg_gc(Gtk::STATE_NORMAL),
				pixbuf->scale_simple(get_width(), get_height(), Gdk::INTERP_BILINEAR),
				0, 0, 0, 0, -1 , -1, Gdk::RGB_DITHER_NONE, 0, 0);

		Gdk::Point rect[4], barcode_center;
		string str;

		bool res = decode_barcode(pixbuf, rect, str, 100);

		if (res)
		{
			Cairo::RefPtr<Cairo::Context> cr = drawable->create_cairo_context();

			cr->set_line_width(3);
			cr->set_source_rgb(1, 0.0, 0.0);

			cr->move_to(0,0);
			cr->line_to(10, 10);
			cr->stroke();

			cr->move_to(rect[0].get_x(), rect[0].get_y() );
			cr->line_to(rect[1].get_x(), rect[1].get_y() );
			cr->line_to(rect[2].get_x(), rect[2].get_y() );
			cr->line_to(rect[3].get_x(), rect[3].get_y() );
			cr->line_to(rect[0].get_x(), rect[0].get_y() );

			cr->stroke();

			BarcodePropertyEstimation distance(get_width(), get_height(),
					rect[0].get_x(), rect[0].get_y(),
					rect[1].get_x(), rect[1].get_y(),
					rect[2].get_x(), rect[2].get_y(),
					rect[3].get_x(), rect[3].get_y());


			Glib::RefPtr<Pango::Layout> pango_layout = Pango::Layout::create(cr);

			Pango::FontDescription desc("Arial Rounded MT Bold");
			desc.set_size(14 * PANGO_SCALE);
			pango_layout->set_font_description(desc);
			pango_layout->set_alignment(Pango::ALIGN_CENTER);

			double dir = distance.calcDirection();
			string dir_str;

			if (dir > 2.3)
				dir_str = "right";
			else if (dir < -2.3)
				dir_str = "left";
			else
				dir_str = "front";

			double dist = distance.calcDistance();
			string text = "Object ID: " + str +
					"\nDistance: " + lexical_cast<string>(int(dist * 100) / 100) + "." +
					lexical_cast<string>(int(dist * 100) % 100) +
					"\nDirection: " + dir_str;

			pango_layout->set_text(text);

			Gdk::Point* max_x, *min_x, *max_y, *min_y;
			boost::function<bool (Gdk::Point, Gdk::Point)> compare;

			compare = bind(less<int>(), bind(&Gdk::Point::get_x, _1), bind(&Gdk::Point::get_x, _2));
			max_x = max_element(rect, rect + 4, compare);
			min_x =	min_element(rect, rect + 4, compare);

			compare = bind(less<int>(), bind(&Gdk::Point::get_y, _1), bind(&Gdk::Point::get_y, _2));
			max_y = max_element(rect, rect + 4, compare);
			min_y =	min_element(rect, rect + 4, compare);

			barcode_center = Gdk::Point(
					(max_x->get_x() + min_x->get_x()) / 2,
					(max_y->get_y() + min_y->get_y()) / 2);

			const Pango::Rectangle extent = pango_layout->get_pixel_logical_extents();

			cr->move_to(get_width() - extent.get_width() - 10,
					get_height() - extent.get_height() - 10);

			pango_layout->show_in_cairo_context(cr);

			DetectedBarcode barcode(str, dir_str, distance.calcDistance());
			vector<DetectedBarcode> vec;
			vec.push_back(barcode);

			ostringstream ostr;
			xmlpp::Document document1;

			listDetectedBarcodesToXML(document1.create_root_node("ObjectList"), vec);

			document1.write_to_stream(ostr, "UTF-8");

			mut.lock();
			strXML = ostr.str();
			mut.unlock();

		}

		return true;
	}

	bool decode_barcode(Glib::RefPtr<Gdk::Pixbuf>& pixbuf, Gdk::Point rect[4], string& str, long timeout_msec = 0)
	{
		DmtxImage* img = dmtxImageCreate(pixbuf->get_pixels(), pixbuf->get_width(), pixbuf->get_height(), DmtxPack24bppRGB);
		bool ret = false;
		if (img)
		{
		    DmtxVector2 p00, p10, p11, p01;
			DmtxTime timeout;

			timeout = dmtxTimeAdd(dmtxTimeNow(), timeout_msec);
			DmtxDecode* dec = dmtxDecodeCreate(img, 1);
			if (dec)
			{
				DmtxRegion* reg = dmtxRegionFindNext(dec, &timeout);

				if (reg)
				{
					DmtxMessage *msg = dmtxDecodeMatrixRegion(dec, reg, DmtxUndefined);
					if (msg)
					{
						str.assign(msg->output, msg->output + msg->outputIdx);
						std::cout << "Output: " << msg->output << " Output index: " << msg->outputIdx << '\n';
						dmtxMessageDestroy(&msg);
						ret = true;
					}
					p00.X = p00.Y = p10.Y = p01.X = 0.0;
					p10.X = p01.Y = p11.X = p11.Y = 1.0;

					dmtxMatrix3VMultiplyBy(&p00, reg->fit2raw);
					dmtxMatrix3VMultiplyBy(&p10, reg->fit2raw);
					dmtxMatrix3VMultiplyBy(&p11, reg->fit2raw);
					dmtxMatrix3VMultiplyBy(&p01, reg->fit2raw);

					rect[0] = Gdk::Point((unsigned int)(p00.X + 0.5),
						(unsigned int)(get_height() - 1 - (int)(p00.Y + 0.5)));
					rect[1] = Gdk::Point((unsigned int)(p01.X + 0.5),
						(unsigned int)(get_height() - 1 - (int)(p01.Y + 0.5)));
					rect[2] = Gdk::Point((unsigned int)(p11.X + 0.5),
						(unsigned int)(get_height() - 1 - (int)(p11.Y + 0.5)));
					rect[3] = Gdk::Point((unsigned int)(p10.X + 0.5),
						(unsigned int)(get_height() - 1 - (int)(p10.Y + 0.5)));

					dmtxRegionDestroy(&reg);
				}
				dmtxDecodeDestroy(&dec);
			}
			dmtxImageDestroy(&img);
		}
		return ret;
	}
};


class HelloWorld : public Gtk::Window
{
public:
	HelloWorld();
	virtual ~HelloWorld(){};

protected:
	bool on_timer_update_image();

	void decode_barcode(Glib::RefPtr<Gdk::Pixbuf>& pixbuf, long timeout_msec);

	VideoWidget m_VW;
	Gtk::Button m_Button;
	Gtk::VBox m_VBox;
};



HelloWorld::HelloWorld()
{
	set_border_width(10);

	Glib::RefPtr<Gdk::Pixbuf> pixbuf;

	m_VBox.pack_start(m_VW);

	Glib::signal_timeout().connect(sigc::mem_fun(this, &HelloWorld::on_timer_update_image), 67);
	boost::thread thread(boost::bind(&send_to_client, ref(m_VW.strXML)));
	add(m_VBox);
	show_all_children();
}

bool HelloWorld::on_timer_update_image()
{
    Glib::RefPtr<Gdk::Window> win = get_window();
    if (win)
    {
    	int top_x, top_y, bottom_x, bottom_y;

    	m_VW.translate_coordinates(*this, 0, 0, top_x, top_y);
    	m_VW.translate_coordinates(*this, m_VW.get_width(), m_VW.get_height(), bottom_x, bottom_y);
        Gdk::Rectangle r(top_x, top_y, bottom_x, bottom_y);
        win->invalidate_rect(r, true);
    }
	return true;
}

int main(int argc, char **argv)
{
	Gst::init();
	Gtk::Main kit(argc, argv);

	HelloWorld h;
	Gtk::Main::run(h);

	return 0;
}
