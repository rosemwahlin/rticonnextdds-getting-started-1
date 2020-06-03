/*
 * (c) Copyright, Real-Time Innovations, 2020.  All rights reserved.
 * RTI grants Licensee a license to use, modify, compile, and create derivative
 * works of the software solely for use with RTI Connext DDS. Licensee may
 * redistribute copies of the software provided that all such copies are subject
 * to this license. The software is provided "as is", with no warranty of any
 * type, including any warranty for fitness for any purpose. RTI is under no
 * obligation to maintain or support the software. RTI shall not be liable for
 * any incidental or consequential damages arising out of the use or inability
 * to use the software.
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "temperature.h"
#include "temperatureSupport.h"
#include "ndds/ndds_cpp.h"
#include "application.h"

using namespace application;

static int shutdown(
        DDSDomainParticipant *participant,
        const char *shutdown_message,
        int status);

// Exercise #2.6: Add function to process ChocolateLotState data
// ----------------------------------------------------------------------
// Process data. Returns number of samples processed.
void process_data(ChocolateLotStateDataReader *lot_state_reader)
{
    ChocolateLotStateSeq data_seq;
    DDS_SampleInfoSeq info_seq;

    // Take available data from DataReader's queue
    DDS_ReturnCode_t retcode = lot_state_reader->take(data_seq, info_seq);

    if (retcode != DDS_RETCODE_OK) {
        std::cerr << "take error " << retcode << std::endl;
        return;
    }

    // Iterate over all available data
    for (int i = 0; i < data_seq.length(); ++i) {
        // Check if a sample is an instance lifecycle event
        if (!info_seq[i].valid_data) {
            std::cout << "Received instance state notification" << std::endl;
            continue;
        }
        // Print data
        ChocolateLotStateTypeSupport::print_data(&data_seq[i]);
    }
    // Data sequence was loaned from middleware for performance.
    // Return loan when application is finished with data.
    lot_state_reader->return_loan(data_seq, info_seq);

}
// ----------------------------------------------------------------------


// Exercise #2.5: Add function that waits for data in a new thread
// ----------------------------------------------------------------------
void wait_for_data(void *reader)
{
    DDS_ReturnCode_t retcode = DDS_RETCODE_OK;

    DDSDataReader *generic_reader = 
            (DDSDataReader *)reader;

    // A narrow is a cast from a generic DataReader to one that is specific
    // to your type. Use the type specific DataReader to read data
    ChocolateLotStateDataReader *lot_state_reader =
            ChocolateLotStateDataReader::narrow(generic_reader);
    if (lot_state_reader == NULL) {
        return;
    }

    // Get status condition: Each entity has a Status Condition, which
    // gets triggered when a status becomes true
    DDSStatusCondition *status_condition = generic_reader->get_statuscondition();
    if (status_condition == NULL) {
        return;
    }

    // Enable only the status we are interested in:
    //   DDS_DATA_AVAILABLE_STATUS
    retcode = status_condition->set_enabled_statuses(DDS_DATA_AVAILABLE_STATUS);
    if (retcode != DDS_RETCODE_OK) {
        return;
    }

    // Create the WaitSet and attach the Status Condition to it. The WaitSet
    // will be woken when the condition is triggered.
    DDSWaitSet waitset;
    retcode = waitset.attach_condition(status_condition);
    if (retcode != DDS_RETCODE_OK) {
        return;
    }

    // wait() blocks execution of the thread until one or more attached
    // Conditions become true, or until a user-specified timeout expires.
    while (!shutdown_requested) {
        DDSConditionSeq active_conditions_seq;

        DDS_Duration_t wait_timeout = { 4, 0 };
        retcode = waitset.wait(active_conditions_seq, wait_timeout);

        // You get a timeout if no conditions were triggered before the timeout
        if (retcode == DDS_RETCODE_TIMEOUT) {
            std::cout << "Wait timed out after 4 seconds." << std::endl;
            continue;
        } else if (retcode != DDS_RETCODE_OK) {
            std::cerr << "wait returned error: " << retcode << std::endl;
            break;
        }
        // Get the status changes to check which status condition
        // triggered the WaitSet to wake
        DDS_StatusMask triggeredmask = lot_state_reader->get_status_changes();

        // If the status is "Data Available"
        if (triggeredmask & DDS_DATA_AVAILABLE_STATUS) {
            process_data(lot_state_reader);
        }
    }
}
// ----------------------------------------------------------------------


int run_example(
        unsigned int domain_id,
        unsigned int sample_count,
        const char *sensor_id)
{
    // Connext DDS setup
    // -----------------
    // A DomainParticipant allows an application to begin communicating in
    // a DDS domain. Typically there is one DomainParticipant per application.
    // DomainParticipant QoS is configured in USER_QOS_PROFILES.xml
    DDSDomainParticipant *participant =
            DDSTheParticipantFactory->create_participant(
                    domain_id,
                    DDS_PARTICIPANT_QOS_DEFAULT,
                    NULL /* listener */,
                    DDS_STATUS_MASK_NONE);
    if (participant == NULL) {
        return shutdown(participant, "create_participant error", EXIT_FAILURE);
    }
    DDS_ReturnCode_t retcode = DDS_RETCODE_OK;

    // Exercise #2.1: Register the ChocolateLotstate type, and add a
    // ChocolateLotstate Topic
    // ----------------------------------------------------------------------
    // Register the datatype to use when creating the Topic
    const char *lot_type_name = ChocolateLotStateTypeSupport::get_type_name();
    retcode =
            ChocolateLotStateTypeSupport::register_type(participant, lot_type_name);
    if (retcode != DDS_RETCODE_OK) {
        return shutdown(participant, "register_type error", EXIT_FAILURE);
    }

    // A Topic has a name and a datatype. Create a Topic called
    // "ChocolateTemperature" with your registered data type
    DDSTopic *chocolate_lot_topic = participant->create_topic(
            "ChocolateLotState",
            lot_type_name,
            DDS_TOPIC_QOS_DEFAULT,
            NULL /* listener */,
            DDS_STATUS_MASK_NONE);
    if (chocolate_lot_topic == NULL) {
        return shutdown(participant, "create_topic error", EXIT_FAILURE);
    }
    // ----------------------------------------------------------------------

    // Register the datatype to use when creating the Topic
    const char *type_name = TemperatureTypeSupport::get_type_name();
    retcode =
            TemperatureTypeSupport::register_type(participant, type_name);
    if (retcode != DDS_RETCODE_OK) {
        return shutdown(participant, "register_type error", EXIT_FAILURE);
    }

    // A Topic has a name and a datatype. Create a Topic called
    // "ChocolateTemperature" with your registered data type
    DDSTopic *topic = participant->create_topic(
            "ChocolateTemperature",
            type_name,
            DDS_TOPIC_QOS_DEFAULT,
            NULL /* listener */,
            DDS_STATUS_MASK_NONE);
    if (topic == NULL) {
        return shutdown(participant, "create_topic error", EXIT_FAILURE);
    }

    // Exercise #2.2: Add a Subscriber
    // ----------------------------------------------------------------------
    // A Subscriber allows an application to create one or more DataReaders
    // Subscriber QoS is configured in USER_QOS_PROFILES.xml
    DDSSubscriber *subscriber = participant->create_subscriber(
            DDS_SUBSCRIBER_QOS_DEFAULT,
            NULL /* listener */,
            DDS_STATUS_MASK_NONE);
    if (subscriber == NULL) {
        shutdown(participant, "create_subscriber error", EXIT_FAILURE);
    }
    // ----------------------------------------------------------------------

    // A Publisher allows an application to create one or more DataWriters
    // Publisher QoS is configured in USER_QOS_PROFILES.xml
    DDSPublisher *publisher = participant->create_publisher(
            DDS_PUBLISHER_QOS_DEFAULT,
            NULL /* listener */,
            DDS_STATUS_MASK_NONE);
    if (publisher == NULL) {
        return shutdown(participant, "create_publisher error", EXIT_FAILURE);
    }

    // Exercise #2.3: Create a DataReader
    // ----------------------------------------------------------------------
    // This DataReader reads data of type Temperature on Topic
    // "ChocolateTemperature". DataReader QoS is configured in
    // USER_QOS_PROFILES.xml
    DDSDataReader *reader = subscriber->create_datareader(
            chocolate_lot_topic,
            DDS_DATAREADER_QOS_DEFAULT,
            NULL,
            DDS_STATUS_MASK_NONE);
    if (reader == NULL) {
        shutdown(participant, "create_datareader error", EXIT_FAILURE);
    }
    // ----------------------------------------------------------------------


    // Exercise #2.4: Create a thread that will wait for data
    // ----------------------------------------------------------------------
    OSThread thread((ThreadFunction)wait_for_data, (void *)reader);
    thread.run();
    // ----------------------------------------------------------------------


    // This DataWriter writes data on Topic "ChocolateTemperature"
    // DataWriter QoS is configured in USER_QOS_PROFILES.xml
    DDSDataWriter *writer = publisher->create_datawriter(
            topic,
            DDS_DATAWRITER_QOS_DEFAULT,
            NULL /* listener */,
            DDS_STATUS_MASK_NONE);
    if (writer == NULL) {
        return shutdown(participant, "create_datawriter error", EXIT_FAILURE);
    }

    // A narrow is a cast from a generic DataWriter to one that is specific
    // to your type. Use the type specific DataWriter to write()
    TemperatureDataWriter *temperature_writer =
            TemperatureDataWriter::narrow(writer);
    if (temperature_writer == NULL) {
        return shutdown(participant, "DataWriter narrow error", EXIT_FAILURE);
    }

    // Create data sample for writing
    Temperature *sample = TemperatureTypeSupport::create_data();
    if (sample == NULL) {
        return shutdown(
                participant,
                "TemperatureTypeSupport::create_data error",
                EXIT_FAILURE);
    }

    // Main loop, write data
    // ---------------------
    for (unsigned int samples_written = 0;
         !shutdown_requested && samples_written < sample_count;
         ++samples_written) {
        // Modify the data to be written here
        snprintf(sample->sensor_id, 255, "%s", sensor_id);
        sample->degrees = rand() % 3 + 30;  // Random number between 30 and 32


        std::cout << "Writing ChocolateTemperature, count " << samples_written
                  << std::endl;
        retcode = temperature_writer->write(*sample, DDS_HANDLE_NIL);
        if (retcode != DDS_RETCODE_OK) {
            std::cerr << "write error " << retcode << std::endl;
        }

        // Exercise: Change this to sleep 100 ms in between writing temperatures
        DDS_Duration_t send_period = { 4, 0 };
        NDDSUtility::sleep(send_period);
    }

    // Exercise #2.7: Add thread join to wait for reading thread to finish
    // ----------------------------------------------------------------------
    // Wait for reader thread to finish
    thread.join();
    // ----------------------------------------------------------------------

    // Cleanup
    // -------
    // Delete data sample
    retcode = TemperatureTypeSupport::delete_data(sample);
    if (retcode != DDS_RETCODE_OK) {
        std::cerr << "TemperatureTypeSupport::delete_data error " << retcode
                  << std::endl;
    }

    // Delete all entities (DataWriter, Topic, Publisher, DomainParticipant)
    return shutdown(participant, "shutting down", EXIT_SUCCESS);
}

// Delete all entities
static int shutdown(
        DDSDomainParticipant *participant,
        const char *shutdown_message,
        int status)
{
    DDS_ReturnCode_t retcode;

    std::cout << shutdown_message << std::endl;

    if (participant != NULL) {
        // This includes everything created by this Participant, including
        // DataWriters, Topics, Publishers. (and Subscribers and DataReaders)
        retcode = participant->delete_contained_entities();
        if (retcode != DDS_RETCODE_OK) {
            std::cerr << "delete_contained_entities error " << retcode
                      << std::endl;
            status = EXIT_FAILURE;
        }

        retcode = DDSTheParticipantFactory->delete_participant(participant);
        if (retcode != DDS_RETCODE_OK) {
            std::cerr << "delete_participant error " << retcode << std::endl;
            status = EXIT_FAILURE;
        }
    }

    return status;
}

int main(int argc, char *argv[])
{
    // Parse arguments and handle control-C
    ApplicationArguments arguments;
    parse_arguments(arguments, argc, argv);
    if (arguments.parse_result == PARSE_RETURN_EXIT) {
        return EXIT_SUCCESS;
    } else if (arguments.parse_result == PARSE_RETURN_FAILURE) {
        return EXIT_FAILURE;
    }
    setup_signal_handlers();

    // Sets Connext verbosity to help debugging
    NDDSConfigLogger::get_instance()->set_verbosity(arguments.verbosity);

    int status = run_example(
            arguments.domain_id,
            arguments.sample_count,
            arguments.sensor_id);

    // Releases the memory used by the participant factory.  Optional at
    // application shutdown
    DDS_ReturnCode_t retcode = DDSDomainParticipantFactory::finalize_instance();
    if (retcode != DDS_RETCODE_OK) {
        std::cerr << "finalize_instance error " << retcode << std::endl;
        status = EXIT_FAILURE;
    }

    return status;
}
