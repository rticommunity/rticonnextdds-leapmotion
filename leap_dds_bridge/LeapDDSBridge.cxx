/* LeapDDSBridge.cxx

       
modification history
------------ -------       
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#ifdef RTI_VX653
#include <vThreadsData.h>
#endif
#include "LeapTypes.h"
#include "LeapTypesSupport.h"
#include "ndds/ndds_cpp.h"
#include "Leap.h"
#include <cctype>

/* Noise */
#define LEAP_POINTABLES_TO_DDS
#define LEAP_HANDS_TO_DDS
#define LEAP_GESTURES_TO_DDS

#define LEAP_APPNOUNCE_FRAMERATE
#define LEAP_APPNOUNCE_COUNTS

/* Entities */
struct DDSEntities {
    DDSDomainParticipant * p;
    DDS_Long               domainId;
    DDS_Long               appId;

    DDSTopic             * t_pointables;
    DDSDataWriter        * w_pointables;
    PointableType        * i_pointables;
    DDS_Long               pointablesCounter;

    DDSTopic             * t_hands;
    DDSDataWriter        * w_hands;
    HandType             * i_hands;
    DDS_Long               handsCounter;

    DDSTopic             * t_gestures;
    DDSDataWriter        * w_gestures;
    GestureType          * i_gestures;
    DDS_Long               gesturesCounter;
};

/* Cleanup */
static int app_shutdown(
    DDSEntities *entities)
{
    DDS_ReturnCode_t retcode;
    int status = 0;

    if (entities->i_gestures != NULL) {
        retcode = GestureTypeTypeSupport::delete_data(entities->i_gestures);
        if (retcode != DDS_RETCODE_OK) {
            printf("GestureTypeTypeSupport::delete_data (i_gestures) error %d\n", retcode);
        } 
        entities->i_gestures = NULL;
    }
    if (entities->i_hands != NULL) {
        retcode = HandTypeTypeSupport::delete_data(entities->i_hands);
        if (retcode != DDS_RETCODE_OK) {
            printf("HandTypeTypeSupport::delete_data (i_hands) error %d\n", retcode);
        } 
        entities->i_hands = NULL;
    }
    if (entities->i_pointables != NULL) {
        retcode = PointableTypeTypeSupport::delete_data(entities->i_pointables);
        if (retcode != DDS_RETCODE_OK) {
            printf("PointableTypeTypeSupport::delete_data (i_pointables) error %d\n", retcode);
        } 
        entities->i_pointables = NULL;
    }

    if (entities->p != NULL) {
        retcode = entities->p->delete_contained_entities();
        if (retcode != DDS_RETCODE_OK) {
            printf("delete_contained_entities error a %d\n", retcode);
            status = -1;
        }

        retcode = DDSTheParticipantFactory->delete_participant(entities->p);
        if (retcode != DDS_RETCODE_OK) {
            printf("delete_participant error a %d\n", retcode);
            status = -1;
        }
        entities->p = NULL;
        memset(entities, 0x0, sizeof(DDSEntities));
    }

    /* RTI Connext provides finalize_instance() method on
       domain participant factory for people who want to release memory used
       by the participant factory. Uncomment the following block of code for
       clean destruction of the singleton. */
/*
    retcode = DDSDomainParticipantFactory::finalize_instance();
    if (retcode != DDS_RETCODE_OK) {
        printf("finalize_instance error %d\n", retcode);
        status = -1;
    }
*/

    return status;
}

/* Leap Frame handler/listener */
class LeapListener : public Leap::Listener {
public:
    DDSEntities * entities;

    void init (DDSEntities *entities) {
        LeapListener::entities = entities;
        entities->gesturesCounter = 0;
        entities->handsCounter = 0;
        entities->pointablesCounter = 0;
    }

    virtual void onInit(const Leap::Controller&) 
    {
    }

    virtual void onConnect(const Leap::Controller&) 
    {
    }

    virtual void onDisconnect(const Leap::Controller&) 
    {
    }

    void copyVector (VectorType *to, Leap::Vector from) {
        to->x = from.x;
        to->y = from.y;
        to->z = from.z;
    }

    void handFrameToInstance(Leap::Hand *h, HandType *instance) {
        instance->id = h->id();
        instance->isValid = h->isValid();

        Leap::PointableList pl = h->pointables();
        instance->pointables.length(pl.count());

        for (int i = 0; i < pl.count(); i++) {
            PointableType * pt = &instance->pointables[i];
            Leap::Pointable p = pl[i];
            RelativeToList relat = PELETON;
            if (p == pl.frontmost()) relat = FRONT_MOST;
            if (p == pl.leftmost()) relat = LEFT_MOST;
            if (p == pl.rightmost()) relat = RIGHT_MOST;
            pointableFrameToInstance(&p, pt);
            pt->isForemost = (relat == FRONT_MOST ? true : false);
            pt->relation = relat;
        }
        copyVector(&instance->palmPosition, h->palmPosition());
        copyVector(&instance->palmVelocity, h->palmVelocity());
        copyVector(&instance->palmDirection, h->direction()); /* unit vector */
        copyVector(&instance->palmStabilized, h->stabilizedPalmPosition());
        copyVector(&instance->palmNormal, h->palmNormal());
        copyVector(&instance->sphereCenter, h->sphereCenter());
        instance->sphereRadius = h->sphereRadius();
        instance->timeVisible = h->timeVisible();
    }

    void pointableFrameToInstance(Leap::Pointable *p, PointableType *instance) {
        instance->id = p->id();
        instance->hand = p->hand().id();
        instance->isFinger = p->isFinger();
        instance->isTool = p->isTool();
        instance->isValid = p->isValid();
        instance->isUnused = true;
        instance->isForemost = false;
        instance->relation = PELETON;
        instance->timeVisible = p->timeVisible();
        copyVector(&instance->tipDirection, p->direction());
        instance->tipLength = p->length();
        copyVector(&instance->tipPosition, p->tipPosition());
        copyVector(&instance->tipStabilized, p->stabilizedTipPosition());
        copyVector(&instance->tipVelocity, p->tipVelocity());
        instance->tipWidth = p->width();
        instance->touchDistance = p->touchDistance();
        instance->touchZone = (Zone)(int)p->touchZone();
    }

    DDS_Long handleHands(const Leap::Controller& controller) {
        DDS_InstanceHandle_t instance_handle = DDS_HANDLE_NIL;
        HandTypeDataWriter *writer = HandTypeDataWriter::narrow(entities->w_hands);
        HandType *instance = entities->i_hands;
        DDS_Long ret = 0;

        if (instance == NULL) return ret;

        Leap::Frame frame = controller.frame();
        if (frame.isValid() == false) { return ret; }

        // appId set on instance creation, elsewhere
        instance->frame = frame.id();

        Leap::HandList hands = frame.hands();

        for (int i = 0; i < hands.count(); i++) {
            Leap::Hand h = hands[i];
            handFrameToInstance(&h, instance);
            writer->write(*instance, instance_handle);
            entities->handsCounter++;
            ret++;
        }
        return ret;
    }

    DDS_Long handleGestures(const Leap::Controller& controller) {
        DDS_InstanceHandle_t instance_handle = DDS_HANDLE_NIL;
        GestureTypeDataWriter *writer = GestureTypeDataWriter::narrow(entities->w_gestures);
        GestureType *instance = entities->i_gestures;
        DDS_Long ret = 0;

        if (instance == NULL) return ret;
        Leap::Frame frame = controller.frame();
        if (frame.isValid() == false) { return ret; }

        instance->frame = frame.id();

        Leap::GestureList gestures = frame.gestures();
        Leap::HandList hands = frame.hands();
        Leap::PointableList pointables = frame.pointables();

        // move guts to gestureFrameToInstance(*,*)
        for (int i = 0; i < gestures.count(); i++) {
            Leap::Gesture gest = gestures[i];
            instance->isValid = gest.isValid();
            instance->durMSec = gest.duration();
            instance->durSec = gest.durationSeconds();
            instance->id = gest.id();
            for (int j = 0; j < hands.count(); j++) {
                Leap::Hand h = hands[j];
                handFrameToInstance(&h, &instance->hands[j]);
            }
            for (int j = 0; j < pointables.count(); j++) {
                Leap::Pointable p = pointables[j];
                pointableFrameToInstance(&p, &instance->pointables[j]);
            }
            instance->state = (State)gest.state();
            instance->type = (Type)gest.type();
            ret++;
        }
        return ret;
    }

    DDS_Long handlePointables(const Leap::Controller& controller) {
        DDS_InstanceHandle_t instance_handle = DDS_HANDLE_NIL;
        PointableTypeDataWriter *writer = PointableTypeDataWriter::narrow(entities->w_pointables);
        PointableType *instance = entities->i_pointables;
        DDS_Long ret = 0;

        if (instance == NULL) return (ret);

        Leap::Frame frame = controller.frame();
        if (frame.isValid() == false) { return (ret); }
        
        // appId set on instance creation, elsewhere
        instance->frame = frame.id();

        Leap::PointableList pointables = frame.pointables();

        for (int i = 0; i < pointables.count(); i++) {
            Leap::Pointable p = pointables[i];
            RelativeToList relat = PELETON;
            if (p == pointables.frontmost()) relat = FRONT_MOST;
            if (p == pointables.leftmost()) relat = LEFT_MOST;
            if (p == pointables.rightmost()) relat = RIGHT_MOST;
            pointableFrameToInstance(&p, instance);
            instance->isForemost = (relat == FRONT_MOST ? true : false);
            instance->relation = relat;
            writer->write(*instance, instance_handle);
            entities->pointablesCounter++;
            ret++;
        }
        return ret;
    }

    void onFrame(const Leap::Controller& controller);
};

void LeapListener::onFrame(const Leap::Controller& controller)
{
    static DDS_Long frames = 0;
    static std::time_t sepoch = std::time(NULL);
    static std::time_t now = std::time(NULL);

    DDS_Long i_count = 0;
    DDS_Long h_count = 0;
    DDS_Long g_count = 0;
#ifdef LEAP_POINTABLES_TO_DDS
    i_count += handlePointables(controller);
#endif
#ifdef LEAP_HANDS_TO_DDS
    h_count += (100 * handleHands(controller));
#endif
#ifdef LEAP_GESTURES_TO_DDS
    g_count += (10000 * handleGestures(controller));
#endif
#ifdef LEAP_ANNOUNCE_COUNTS
    frames++;
    if ((frames % 3000) == 0) { 
        printf("Frames %i, Counts: [i] %i [h] %i [g] %i\n", frames, i_count, h_count, g_count); 
    } 
#endif
#ifdef LEAP_ANNOUNCE_FRAMERATE
    std::time_t check = std::time(NULL);
    if (check != now) {
        now = check;
        DDS_Double fps = (1000.0) * ((now-sepoch)/(double)frames);
        printf("sepoch: %li, now: %li, frames: %i, FPS:  %f\n", sepoch, now, frames, fps); 
    }
#endif
}


/* Setup: Participant */
extern "C" DDSDomainParticipant * publisher_main(int domainId, DDSEntities * entities)
{
    DDS_ReturnCode_t retcode;
    DDSDomainParticipant *participant = NULL;

    DDSTopic *topic = NULL;
    const char *type_name = NULL;
 
    entities->domainId = domainId; // used for appId also

    /* To customize participant QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    participant = DDSTheParticipantFactory->create_participant(
        domainId, DDS_PARTICIPANT_QOS_DEFAULT, 
        NULL /* listener */, DDS_STATUS_MASK_NONE);
    if (participant == NULL) {
        printf("create_participant error\n");
        app_shutdown(entities);
        return NULL;
    }
    entities->p = participant;
    
    /* Register type before creating topic */
    type_name = PointableTypeTypeSupport::get_type_name();
    retcode = PointableTypeTypeSupport::register_type(
        participant, type_name);
    if (retcode != DDS_RETCODE_OK) {
        printf("register_type error PointableType %d\n", retcode);
        app_shutdown(entities);
        return NULL;
    }
    
    /* To customize topic QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    topic = participant->create_topic(
        "Leap::Pointable",
        type_name, DDS_TOPIC_QOS_DEFAULT, NULL /* listener */,
        DDS_STATUS_MASK_NONE);
    if (topic == NULL) {
        printf("create_topic Leap::Pointable error\n");
        app_shutdown(entities);
        return NULL;
    }
    entities->t_pointables = topic;

        /* Register type before creating topic */
    type_name = HandTypeTypeSupport::get_type_name();
    retcode = HandTypeTypeSupport::register_type(
        participant, type_name);
    if (retcode != DDS_RETCODE_OK) {
        printf("register_type error HandType %d\n", retcode);
        app_shutdown(entities);
        return NULL;
    }
    
    /* To customize topic QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    topic = participant->create_topic(
        "Leap::Hand",
        type_name, DDS_TOPIC_QOS_DEFAULT, NULL /* listener */,
        DDS_STATUS_MASK_NONE);
    if (topic == NULL) {
        printf("create_topic Leap::Hand error\n");
        app_shutdown(entities);
        return NULL;
    }
    entities->t_hands = topic;

    /* Register type before creating topic */
    type_name = GestureTypeTypeSupport::get_type_name();
    retcode = GestureTypeTypeSupport::register_type(
        participant, type_name);
    if (retcode != DDS_RETCODE_OK) {
        printf("register_type error GestureType %d\n", retcode);
        app_shutdown(entities);
        return NULL;
    }
    
    /* To customize topic QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    topic = participant->create_topic(
        "Leap::Gesture",
        type_name, DDS_TOPIC_QOS_DEFAULT, NULL /* listener */,
        DDS_STATUS_MASK_NONE);
    if (topic == NULL) {
        printf("create_topic Leap::Gesture error\n");
        app_shutdown(entities);
        return NULL;
    }
    entities->t_gestures = topic;
    
    return (participant);
}

/* Setup: Writers */
extern "C" void writer_main(DDSEntities *entities) {
    DDSDataWriter *writer = NULL;
    
        /* To customize data writer QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    writer = entities->p->create_datawriter(
        entities->t_pointables, DDS_DATAWRITER_QOS_DEFAULT, NULL /* listener */,
        DDS_STATUS_MASK_NONE);
    if (writer == NULL) {
        printf("create_datawriter error Pointables\n");
        app_shutdown(entities);
        return;
    }
    if (PointableTypeDataWriter::narrow(writer) == NULL) {
        printf("DataWriter narrow error Pointables\n");
        app_shutdown(entities);
        return;
    }
    entities->w_pointables = writer;

        /* To customize data writer QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    writer = entities->p->create_datawriter(
        entities->t_hands, DDS_DATAWRITER_QOS_DEFAULT, NULL /* listener */,
        DDS_STATUS_MASK_NONE);
    if (writer == NULL) {
        printf("create_datawriter error Hands\n");
        app_shutdown(entities);
        return;
    }
    if (HandTypeDataWriter::narrow(writer) == NULL) {
        printf("DataWriter narrow error Hands\n");
        app_shutdown(entities);
        return;
    }
    entities->w_hands = writer;

        /* To customize data writer QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    writer = entities->p->create_datawriter(
        entities->t_gestures, DDS_DATAWRITER_QOS_DEFAULT, NULL /* listener */,
        DDS_STATUS_MASK_NONE);
    if (writer == NULL) {
        printf("create_datawriter error Gestures\n");
        app_shutdown(entities);
        return;
    }
    if (GestureTypeDataWriter::narrow(writer) == NULL) {
        printf("DataWriter narrow error Gestures\n");
        app_shutdown(entities);
        return;
    }
    entities->w_gestures = writer;

    /* Create data sample for writing */

    entities->i_pointables = PointableTypeTypeSupport::create_data();
    if (entities->i_pointables == NULL) {
        printf("PointableTypeTypeSupport::create_data error\n");
        app_shutdown(entities);
        return;
    }
    entities->i_pointables->appId = entities->appId;

    entities->i_hands = HandTypeTypeSupport::create_data();
    if (entities->i_hands == NULL) {
        printf("HandTypeTypeSupport::create_data error\n");
        app_shutdown(entities);
        return;
    }
    entities->i_hands->appId = entities->domainId;

    entities->i_gestures = GestureTypeTypeSupport::create_data();
    if (entities->i_gestures == NULL) {
        printf("GestureTypeTypeSupport::create_data error\n");
        app_shutdown(entities);
        return;
    }
    entities->i_gestures->appId = entities->domainId;

    return;
}

int main(int argc, char *argv[])
{
    int domainId = 112;
    int appId = 1;
    int count = 0;

    Leap::Controller s_controller;
    LeapListener lister;
    
    DDSEntities entities;
    PointableTypeDataWriter *pt_writer = NULL;
    DDS_Duration_t send_period = {15,0};

    if (argc >= 2) {
        domainId = atoi(argv[1]);
    }
    if (argc >= 3) {
        appId = atoi(argv[2]);
    }
    entities.appId = appId;

    /* Uncomment this to turn on additional logging
    NDDSConfigLogger::get_instance()->
        set_verbosity_by_category(NDDS_CONFIG_LOG_CATEGORY_API, 
                                  NDDS_CONFIG_LOG_VERBOSITY_STATUS_ALL);
    */

    lister.init(&entities);

    publisher_main(domainId, &entities);
    writer_main(&entities);

    s_controller.setPolicyFlags(Leap::Controller::POLICY_BACKGROUND_FRAMES);

    s_controller.addListener(lister);

    /* Main loop */
    printf("Leap Data on RTI Connext DDS 5.1.0 on Domain %i\n", domainId);
    printf("Entering wait loop...\n");
    for (count=0; ; ++count) {
        printf("count = %i; hand = %i, pointables = %i\n", count, entities.handsCounter, entities.pointablesCounter);
        NDDSUtility::sleep(send_period);
    }

/*
    retcode = GestureType_writer->unregister_instance(
        *instance, instance_handle);
    if (retcode != DDS_RETCODE_OK) {
        printf("unregister instance error %d\n", retcode);
    }
*/

    /* Delete all entities */
    app_shutdown(&entities);

    return(0);
}


