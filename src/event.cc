#include "bindings.h"
#include <cmath>

// currently active
static int event_count = 0;
// at destruction - size and average size
static int event_size_total = 0;
static int event_size_total_count = 0;

/*
//
// Create an event using oboe's context.
//
Event::Event(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Event>(info) {
    Napi::Env env = info.Env();

    event_count++;
    // TODO BAM consider throwing if oboe_status != 0.
    oboe_status = oboe_event_init(&event, oboe_context_get(), NULL);

}

//
// Create an event using the specified metadata instead of
// oboe's context. Optionally add an edge to the metadata's
// op ID.
//
Event::Event(const oboe_metadata_t* md, bool addEdge) {
    event_count++;
    if (addEdge) {
        // all this does is call oboe_event_init() followed by
        // oboe_event_add_edge().
        oboe_status = oboe_metadata_create_event(md, &event);
    } else {
        // this copies md to the event metadata except for the opId.
        // It creates a new random opId for the event.
        oboe_status = oboe_event_init(&event, md, NULL);
    }
}
// */

Event::~Event() {
    event_count--;
    event_size_total += sizeof(event) + event.bbuf.bufSize;
    event_size_total_count += 1;
    oboe_event_destroy(&event);
}

Napi::FunctionReference Event::constructor;

/**
 * Run at module initialization. Make the constructor and export JavaScript
 * properties and function.
 */
Napi::Object Event::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);

    Napi::Function ctor = DefineClass(env, "Event", {
      InstanceMethod("addInfo", &Event::addInfo),
      InstanceMethod("addEdge", &Event::addEdge),
      InstanceMethod("getMetadata", &Event::getMetadata),
      InstanceMethod("toString", &Event::toString),
      InstanceMethod("getSampleFlag", &Event::getSampleFlag),
      InstanceMethod("setSampleFlagTo", &Event::setSampleFlagTo)
    });

    constructor = Napi::Persistent(ctor);
    constructor.SuppressDestruct();

    exports.Set("Event", ctor);

    return exports;
}

/**
 * JavaScript constructor
 *
 * new Event()
 * new Event(xtrace, addEdge)
 *
 * @param {Metadata|Event|string} xtrace - X-Trace ID to use for creating event
 * @param boolean [addEdge]
 *
 */
Event::Event(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Event>(info) {
    Napi::Env env = info.Env();

    // throw an error if constructor is called as a function without "new"
    if(!info.IsConstructCall()) {
        Napi::Error::New(env, "Event must be called as a constructor").ThrowAsJavaScriptException();
        return;
    }

    oboe_metadata_t omd;

    // Handle the no argument signature.
    if (info.Length() == 0) {
      // oboe_context_get() allocates memory so copy and free up front to minimize
      // memory management headaches later on.
      oboe_metadata_t* context = oboe_context_get();
      omd = *context;
      delete context;
    } else {
      Napi::Object o = info[0].As<Napi::Object>();

      if (Metadata::isMetadata(o)) {
        Metadata* md = Napi::ObjectWrap<Metadata>::Unwrap(o);
        omd = md->metadata;
      } else if (Event::isEvent(o)) {
        Event* ev = Napi::ObjectWrap<Event>::Unwrap(o);
        omd = ev->event.metadata;
      } else if (info[0].IsExternal()) {
        omd = *info[0].As<Napi::External<oboe_metadata_t>>().Data();
      } else if (info[0].IsString()) {
        std::string xtrace = info[0].As<Napi::String>();
        int status = oboe_metadata_fromstr(&omd, xtrace.c_str(), xtrace.length());
        if (status != 0) {
          Napi::Error::New(env, "Event::New - invalid X-Trace ID string").ThrowAsJavaScriptException();
          return;
        }
      } else {
        Napi::TypeError::New(env, "invalid arguments");
        return;
      }
    }

    // here there should be metadata in omd and that's all the information needed in order
    // to create an event.
    event_count++;

    // add an edge unless the user specifies not to.
    bool add_edge = true;
    if (info.Length() >= 2) {
      add_edge = info[1].ToBoolean().Value();
    }

    // copy the metadata to the event except for the op ID. oboe_event_init()
    // will create a new random op ID for the event. (The op ID can be specified
    // using the 3rd argument but there is no benefit to doing so.)
    int oboe_status = oboe_event_init(&this->event, &omd, NULL);

    // if the event init succeeded and an edge must be added do so.
    if (oboe_status == 0 && add_edge) {
      oboe_status = oboe_event_add_edge(&this->event, &omd);
      // if it failed cleanup.
      if (oboe_status != 0) {
        oboe_event_destroy(&this->event);
      }
    }


    /*
    // Now handle the Metadata, Event, or String signatures. All have an optional
    // boolean (default true) to add an edge to the event (points to op ID in metadata).
    if (Metadata::isMetadata(info[0])) {
       Metadata* md = info[0].ToObject().Unwrap<Metadata>();
       mdp = &md->metadata;
    } else if (info[0].IsExternal()) {
        // this is only used by other C++ methods, not JavaScript. They must pass
        // a pointer to an oboe_metadata_t structure.
        mdp = static_cast<oboe_metadata_t*>(info[0].As<Napi::External>()->Value());
    } else if (Event::isEvent(info[0])) {
        Event* e = info[0].ToObject().Unwrap<Event>();
        mdp = &e->event.metadata;
    } else if (info[0].IsString()) {
        std::string xtrace = info[0].As<Napi::String>();
        int status = oboe_metadata_fromstr(&converted_md, *xtrace, xtrace.Length());
        if (status != 0) {
            Napi::Error::New(env, Napi::String::New(env, "Event::New - invalid X-Trace ID string")).ThrowAsJavaScriptException();
            return env.Null();
        }
        mdp = &converted_md;
    } else {
        Napi::TypeError::New(env, Napi::String::New(env, "Event::New - invalid argument")).ThrowAsJavaScriptException();
        return env.Null();
    }

    // handle adding an edge default or specified explicitly?
    bool add_edge = true;
    if (info.Length() >= 2 && info[1].IsBoolean()) {
        add_edge = info[1].As<Napi::Boolean>().Value();
    }

    // now make the event using the metadata specified. in no case is
    // metadata allocated so there is no need to delete it.
    event = new Event(mdp, add_edge);
    event->Wrap(info.This());
    return info.This();
    // */
}

/**
 * C++ callable function to create a JavaScript Event object.
 */
Napi::Object Event::NewInstance(Napi::Env env) {
    Napi::EscapableHandleScope scope(env);

    Napi::Object o = constructor.New({});

    return scope.Escape(napi_value(o)).ToObject();
}

/**
 * C++ callable function to create a JavaScript Event object.
 *
 * This signature includes a boolean for whether an edge is set or not.
 */
Napi::Object Event::NewInstance(Napi::Env env, oboe_metadata_t* omd, bool edge = true) {
    Napi::EscapableHandleScope scope(env);

    Napi::External<oboe_metadata_t> v0 = Napi::External<oboe_metadata_t>::New(env, omd);
    Napi::Value v1 = Napi::Boolean::New(env, edge);

    Napi::Object o = constructor.New({v0, v1});

    return scope.Escape(napi_value(o)).ToObject();

    /*
    const unsigned argc = 2;
    Napi::Value argv[argc] = {
        Napi::External::New(env, &md->metadata),
        Napi::New(env, edge)
    };
    Napi::Function cons = Napi::Function::New(env, constructor);
    // Now invoke the JavaScript callable constructor (Event::New).
    //Napi::Object instance = cons->NewInstance(argc, argv);
    Napi::Object instance = Napi::NewInstance(cons, argc, argv);

    return scope.Escape(instance);
    // */
}


/**
 * C++ callable function to create a JavaScript Event object.
 */
/*
Napi::Object Event::NewInstance(Metadata* md) {
    Napi::EscapableHandleScope scope(env);

    const unsigned argc = 1;
    Napi::Value argv[argc] = { Napi::External::New(env, &md->metadata) };
    Napi::Function cons = Napi::Function::New(env, constructor);
    // Now invoke the JavaScript callable constructor (Event::New).
    //Napi::Object instance = cons->NewInstance(argc, argv);
    Napi::Object instance = Napi::NewInstance(cons, argc, argv);

    return scope.Escape(instance);
}
// */

/**
 * C++ callable function to create a JavaScript Event object.
 */
/*
Napi::Object Event::NewInstance() {
    Napi::EscapableHandleScope scope(env);

    const unsigned argc = 0;
    Napi::Value argv[argc] = {};
    Napi::Function cons = Napi::Function::New(env, constructor);
    //Napi::Object instance = cons->NewInstance(argc, argv);
    Napi::Object instance = Napi::NewInstance(cons, argc, argv);

    return scope.Escape(instance);
}
// */

/*
Napi::Value Event::getEventData(const Napi::CallbackInfo& info) {
    Napi::String active = Napi::String::New(env, "active");
    Napi::String freedBytes = Napi::String::New(env, "freedBytes");
    Napi::String freedCount = Napi::String::New(env, "freedCount");

    Napi::Object o = Napi::Object::New(env);
    (o).Set(active, Napi::Number::New(env, event_count));
    (o).Set(freedBytes, Napi::Number::New(env, event_size_total));
    (o).Set(freedCount, Napi::Number::New(env, event_size_total_count));

    return o;
}
// */

Napi::Value Event::toString(const Napi::CallbackInfo& info) {
    // Unwrap the Event instance from V8 and get the metadata reference.
    oboe_metadata_t* md = &this->event.metadata;
    char buf[OBOE_MAX_METADATA_PACK_LEN];

    int rc;
    // TODO consider accepting an argument to select upper or lower case.
    if (info.Length() == 1 && info[0].ToBoolean().Value()) {
      rc = Metadata::format(md, sizeof(buf), buf) ? 0 : -1;
    } else {
      rc = oboe_metadata_tostr(md, buf, sizeof(buf) - 1);
    }

    return Napi::String::New(info.Env(), rc == 0 ? buf : "");
}

/**
 * JavaScript callable method to set the event's sample flag to the boolean
 * argument.
 *
 * returns the previous value of the flag.
 */
Napi::Value Event::setSampleFlagTo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() != 1) {
        Napi::Error::New(env, "setSampleFlagTo requires one argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    bool previous = this->event.metadata.flags & XTR_FLAGS_SAMPLED;

    // truthy to set it, falsey to clear it
    if (info[0].ToBoolean().Value()) {
        this->event.metadata.flags |= XTR_FLAGS_SAMPLED;
    } else {
        this->event.metadata.flags &= ~XTR_FLAGS_SAMPLED;
    }
    return Napi::Boolean::New(env, previous);
}

/**
 * JavaScript callable method to get the sample flag from
 * the event metadata.
 *
 * returns boolean
 */
Napi::Value Event::getSampleFlag(const Napi::CallbackInfo& info) {
    return Napi::Boolean::New(info.Env(), this->event.metadata.flags & XTR_FLAGS_SAMPLED);
}

/**
 * JavaScript callable method to get a new metadata object with
 * the same metadata as this event.
 *
 * Event.getMetadata()
 *
 * Return a Metadata object with the metadata from this event.
 */
Napi::Value Event::getMetadata(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    oboe_metadata_t* omd = &this->event.metadata;

    Napi::External<oboe_metadata_t> v = Napi::External<oboe_metadata_t>::New(env, omd);
    return Metadata::NewInstance(env, v);
}

/**
 * JavaScript callable method to add an edge to the event.
 *
 * event.addEdge(edge)
 *
 * @param {Event | Metadata | string} X-Trace ID to edge back to
 */
Napi::Value Event::addEdge(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Validate arguments
    if (info.Length() != 1) {
        Napi::TypeError::New(env, "invalid arguments").ThrowAsJavaScriptException();
        return env.Null();
    }

    int status;
    Napi::Object o = info[0].ToObject();
    if (Event::isEvent(o)) {
        Event* e = Napi::ObjectWrap<Event>::Unwrap(o);
        status = oboe_event_add_edge(&this->event, &e->event.metadata);
    } else if (Metadata::isMetadata(o)) {
        Metadata* md = Napi::ObjectWrap<Metadata>::Unwrap(o);
        status = oboe_event_add_edge(&this->event, &md->metadata);
    } else if (info[0].IsString()) {
        std::string str = info[0].As<Napi::String>();
        status = oboe_event_add_edge_fromstr(&this->event, str.c_str(), str.length());
    } else {
        Napi::TypeError::New(env, "invalid metadata").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (status < 0) {
        Napi::Error::New(env, "Failed to add edge").ThrowAsJavaScriptException();
        return env.Null();
    }
    return Napi::Boolean::New(env, true);
}

/**
 * JavaScript method to add info to the event.
 *
 * event.addInfo(key, value)
 *
 * @param {string} key
 * @param {string | number | boolean} value
 */
Napi::Value Event::addInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Validate arguments
    if (info.Length() != 2 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Invalid arguments").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[1].IsString() && !info[1].IsNumber() && !info[1].IsBoolean()) {
        Napi::TypeError::New(env, "Value must be a boolean, string or number").ThrowAsJavaScriptException();
        return env.Null();
    }

    oboe_event_t* event = &this->event;

    int status;

    // Get key string
    std::string key = info[0].As<Napi::String>();


    // Handle boolean values
    if (info[1].IsBoolean()) {
        bool v = info[1].As<Napi::Boolean>().Value();
        status = oboe_event_add_info_bool(event, key.c_str(), v);

    // Handle integer values
    } else if (info[1].IsNumber()) {
        const double v = info[1].As<Napi::Number>();

        // if it is outside IEEE 754 integer range add a double
        if (v > pow(2, 53) - 1 || v < -(pow(2, 53) - 1)) {
          status = oboe_event_add_info_double(event, key.c_str(), v);
        } else {
          status = oboe_event_add_info_int64(event, key.c_str(), v);
        }

    // Handle string values
    } else {
        // Get value string from arguments
        std::string str = info[1].As<Napi::String>();

        // Detect if we should add as binary or a string
        // TODO evaluate using buffers for binary data...
        if (memchr(str.c_str(), '\0', str.length())) {
            status = oboe_event_add_info_binary(event, key.c_str(), str.c_str(), str.length());
        } else {
            status = oboe_event_add_info(event, key.c_str(), str.c_str());
        }
    }

    if (status < 0) {
        Napi::Error::New(env, "Failed to add info").ThrowAsJavaScriptException();
    }

    return Napi::Boolean::New(env, status == 0);
}

/**
 * C++ callable method to determine if object is a JavaScript Event
 * instance.
 *
 */
bool Event::isEvent(Napi::Object o) {
  return o.IsObject() && o.InstanceOf(constructor.Value());
}
