#include <gst/gst.h>
#include <gst/video/videooverlay.h>

//#define USE_PLAYBIN
 
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

int use_playbin()
{
    GstElement* pipeline = gst_element_factory_make("playbin", "video-player");

    if (!pipeline)
    {
        g_printerr("Failed to create pipeline.\n");
        return -1;
    }

    // Set the URI to the file path of your MP4 video
    //g_object_set(pipeline, "uri", "c:\\gstreamer\\colors.mp4", nullptr);
    g_object_set(pipeline, "uri", "file:///c:/gstreamer/colors.mp4", nullptr);


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

int main(int argc, char* argv[]) 
{
    // Initialize GStreamer
    gst_init(nullptr, nullptr);

#ifdef USE_PLAYBIN
    use_playbin();
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
    //g_object_set(source, "location", "c:\\gstreamer\\colors.mp4", nullptr);
    g_object_set(source, "location", "c:/gstreamer/colors.mp4", nullptr);
    // g_object_set(source, "uri", "file:///c:/gstreamer/colors.mp4", nullptr);

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

