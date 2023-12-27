#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <iostream>

//#define USE_PLAYBIN // - define this to use playbin to play the video file
//#define USE_PLAYBINRTSP // - define this to use playbin to play the rtsp stream
 
static gboolean busCallback(GstBus* bus, GstMessage* msg, gpointer data) 
{
    GMainLoop* loop = (GMainLoop*)data;
    
    switch (GST_MESSAGE_TYPE(msg)) 
    {
        case GST_MESSAGE_EOS:
            g_printerr("EOS received from element %s\n", GST_OBJECT_NAME(msg->src));
            g_main_loop_quit(loop);
            break;

        case GST_MESSAGE_ERROR: 
        {
            gchar* debug;
            GError* error;

            gst_message_parse_error(msg, &error, &debug);
            g_free(debug);
            g_printerr("Error message %s received from element %s\n", error->message, GST_OBJECT_NAME(msg->src));
            g_error_free(error);
            g_main_loop_quit(loop);
            break;
        }

        default:
            // Handle other message types if needed
            break;
    }

    return true;  // Continue receiving messages
}

static void on_pad_added(GstElement* element, GstPad* pad, gpointer data)
{
    GstPad* sinkpad;
    GstElement* decoder = (GstElement*)data;

    /* We can now link this pad with the sink pad */
    g_print("Dynamic pad created, linking demuxer/decoder\n");

    sinkpad = gst_element_get_static_pad(decoder, "sink");
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
}

static void cb_new_rtspsrc_pad(GstElement* element, GstPad* pad, gpointer  data)
{
    gchar* name;
    GstCaps* p_caps;
    gchar* description;
    GstElement* p_rtph264depay;

    name = gst_pad_get_name(pad);
    g_print("A new pad %s was created\n", name);

    // here, you would setup a new pad link for the newly created pad
    // sooo, now find that rtph264depay is needed and link them?
    p_caps = gst_pad_get_pad_template_caps(pad);

    description = gst_caps_to_string(p_caps);
    printf("%s\n", p_caps, ", ", description, "\n");
    g_free(description);

    p_rtph264depay = GST_ELEMENT(data);

    // try to link the pads then ...
    if (!gst_element_link_pads(element, name, p_rtph264depay, "sink"))
    {
        printf("Failed to link elements 3\n");
    }

    g_free(name);
}

int use_playbin(std::string filename)
{
    GstElement* pipeline = gst_element_factory_make("playbin", "video-player");

    if (!pipeline)
    {
        g_printerr("Failed to create pipeline.\n");
        return -1;
    }

    // Set the URI to the file path of your MP4 video
    //g_object_set(pipeline, "uri", "file:///c:/gstreamer/colors.mp4", nullptr);
    std::string struri = "file:///" + filename;
    g_object_set(pipeline, "uri", struri.c_str(), nullptr);


    // Set the pipeline to the "playing" state
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // Get the bus for handling messages
    GstBus* bus = gst_element_get_bus(pipeline);

    // Main loop
    GstMessage* msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg != nullptr)
    {
        // Handle messages if needed
        // ...

        gst_message_unref(msg);
    }

    // Clean up resources
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}

int playvid(std::string filename)
{
#ifdef USE_PLAYBIN
    use_playbin(filename);
#else
    gst_debug_set_active(true);
    gst_debug_set_default_threshold(GST_LEVEL_LOG);

    GMainLoop* loop;
    loop = g_main_loop_new(nullptr, false);

    // This works on the command line
    // gst-launch-1.0 -v filesrc location = "c:\\gstreamer\\colors.mp4" ! qtdemux ! h264parse ! avdec_h264 ! videoconvert ! autovideosink
    // gst-launch-1.0 -v filesrc location = "c:\\gstreamer\\colors.mp4" ! qtdemux ! queue ! h264parse ! avdec_h264 ! videoconvert ! autovideosink

    // Create a GStreamer pipeline
    GstElement* pipeline = gst_pipeline_new("video-player");

    // Create elements
    GstElement* source = gst_element_factory_make("filesrc", "file-source");
    GstElement* demuxer = gst_element_factory_make("qtdemux", "demux");
    //GstElement* queue = gst_element_factory_make("queue", "vidqueue");
    GstElement* parser = gst_element_factory_make("h264parse", "vidparse");
    GstElement* decoder = gst_element_factory_make("avdec_h264", "h264-decoder");
    GstElement* videoconvert = gst_element_factory_make("videoconvert", "video-convert");
    //GstElement*  sink = gst_element_factory_make("appsink", "video-output");
    GstElement* videosink = gst_element_factory_make("autovideosink", "video-sink");
    //GstElement* videosink = gst_element_factory_make("d3dvideosink", "video-sink");

    //if (!pipeline || !source || !decoder || !videoconvert || !videosink) 
    if (!pipeline || !source || !demuxer || !parser || !decoder || !videoconvert || !videosink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    // Set the input file path
    //g_object_set(source, "location", "c:/gstreamer/colors.mp4", nullptr);
    g_object_set(source, "location", filename.c_str(), nullptr);

    guint bus_watch_id;
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, busCallback, loop);

    // Add elements to the pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, demuxer, parser, decoder, videoconvert, videosink, nullptr);

#if 0   // we can link them all if the output is a stream. For synchronous file playback, we need syncing and 
    // dynamic linking of the demuxer and parser
// Link elements
    if (gst_element_link_many(source, demuxer, parser, decoder, videoconvert, videosink, nullptr) != true)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }
#else
    if (gst_element_link(source, demuxer) != true)
    {
        g_printerr("Could not link source and demuxer!\n");
        gst_object_unref(pipeline);
        return -1;
    }
    if (gst_element_link_many(parser, decoder, videoconvert, videosink, nullptr) != true)
    {
        g_printerr("Could not link parser,decoder,converter, and sink!\n");
        gst_object_unref(pipeline);
        return -1;
    }
    g_signal_connect(demuxer, "pad-added", G_CALLBACK(on_pad_added), parser);
#endif

    // Set the pipeline to the "playing" state
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    g_main_loop_run(loop);

    // Free resources
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    gst_object_unref(bus);
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);
#endif
    return 0;
}

int use_playbinrtsp(std::string url_rtsp)
{
    GstElement* pipeline = gst_element_factory_make("playbin", "video-player");

    if (!pipeline)
    {
        g_printerr("Failed to create pipeline.\n");
        return -1;
    }

    // Set the URI to the file path of your MP4 video
    g_object_set(pipeline, "uri", url_rtsp.c_str(), nullptr);

    // Set the pipeline to the "playing" state
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // Get the bus for handling messages
    GstBus* bus = gst_element_get_bus(pipeline);

    // Main loop
    GstMessage* msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg != nullptr)
    {
        // Handle messages if needed
        // ...

        gst_message_unref(msg);
    }

    // Clean up resources
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}

int playrtsp(std::string url_rtsp)
{
#ifdef USE_PLAYBINRTSP
    use_playbinrtsp(url_rtsp);
#else
    gst_debug_set_active(true);
    gst_debug_set_default_threshold(GST_LEVEL_LOG);

    GMainLoop* loop;
    loop = g_main_loop_new(nullptr, false);

    // This works on the command line
    // gst-launch-1.0 -v rtspsrc location="rtsp://b03773d78e34.entrypoint.cloud.wowza.com:1935/app-4065XT4Z/80c76e59_stream1" ! 
    // queue max-size-buffers=2 ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! videoscale ! video/x-raw,width=640,height=480 ! autovideosink

    //gst-launch-1.0 -v rtspsrc location = "rtsp://b03773d78e34.entrypoint.cloud.wowza.com:1935/app-4065XT4Z/80c76e59_stream1" !decodebin !autovideosink sync = false
    // Create a GStreamer pipeline
    GstElement* pipeline = gst_pipeline_new("video-player-rtsp");

    // Create elements
    GstElement* source = gst_element_factory_make("rtspsrc", "rtsp-source");
    g_object_set(G_OBJECT(source), "latency", 2000, nullptr);

    //GstElement* queue = gst_element_factory_make("queue", "rtpqueue");
    GstElement* depay = gst_element_factory_make("rtph264depay", "depay");
    GstElement* parser = gst_element_factory_make("h264parse", "vidparse");
    GstElement* filter = gst_element_factory_make("capsfilter", "filter");
    GstElement* decoder = gst_element_factory_make("avdec_h264", "h264-decoder");
    GstElement* videoconvert = gst_element_factory_make("videoconvert", "video-convert");
    //GstElement*  sink = gst_element_factory_make("appsink", "video-output");
    GstElement* videosink = gst_element_factory_make("autovideosink", "video-sink");
    g_object_set(G_OBJECT(videosink), "sync", false, nullptr);

    if (!pipeline || !source || !depay || !parser || !filter || !decoder || !videoconvert || !videosink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    // Set the input file path
    g_object_set(GST_OBJECT(source), "location", url_rtsp.c_str(), nullptr);

    GstCaps* filtercaps = gst_caps_from_string("application/x-rtp");
    g_object_set(G_OBJECT(filter), "caps", filtercaps, NULL);
    gst_caps_unref(filtercaps);

    guint bus_watch_id;
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, busCallback, loop);

    // Add elements to the pipeline
    //gst_bin_add_many(GST_BIN(pipeline), source, depay, parser, decoder, videoconvert, videosink, nullptr);

    gst_bin_add_many(GST_BIN(pipeline), source, depay, nullptr);
    
    // listen for newly created pads
    g_signal_connect(source, "pad-added", G_CALLBACK(cb_new_rtspsrc_pad), depay);
    gst_bin_add_many(GST_BIN(pipeline), parser, nullptr);
    if (!gst_element_link(depay, parser))
    { 
        g_printerr("NOPE - could not link depay and parser\n"); 
        gst_object_unref(pipeline);
        return -1;
    }
   
    gst_bin_add_many(GST_BIN(pipeline), decoder, videoconvert, videosink, nullptr);
    if (!gst_element_link_many(parser, decoder, videoconvert, videosink, nullptr))
    {
        g_printerr("NOPE - could not link parser through sink\n");
        gst_object_unref(pipeline);
        return -1;
    }

    g_signal_connect(depay, "pad-added", G_CALLBACK(on_pad_added), parser);

    // Set the pipeline to the "playing" state
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    g_main_loop_run(loop);

    // Free resources
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    gst_object_unref(bus);
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);
#endif
    return 0;
}

int main(int argc, char* argv[]) 
{
    if (argc < 3) 
    {
        std::cout << "Usage: " << argv[0] << " <command> <other parms>" << std::endl;
        std::cout << "Example to play a video file: gstreamer_playground playvid c:/gstreamer/colors.mp4" << std::endl;
    
        return 1; // Return an error code
    }

    std::cout << "Arguments provided:" << std::endl;
    for (int i = 1; i < argc; ++i) 
    {
        std::cout << "  " << i << ": " << argv[i] << std::endl;
    }

    // Initialize GStreamer
    gst_init(nullptr, nullptr);

    if (std::string(argv[1]) == "playvid")
    { 
        // command-line: playvid c:/gstreamer/colors.mp4
        std::string filename(argv[2]);
        playvid(filename);
    }
    else if (std::string(argv[1]) == "playrtsp")
    {
        // command-line: playrtsp rtsp://b03773d78e34.entrypoint.cloud.wowza.com:1935/app-4065XT4Z/80c76e59_stream1
        std::string rtsp_url(argv[2]);
        playrtsp(rtsp_url);
    }
}
 

