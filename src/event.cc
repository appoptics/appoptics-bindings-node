#include "bindings.h"
#include <cmath>
#include <iomanip>

Napi::FunctionReference Event::constructor;

Event::~Event() {
  // don't ask oboe to clean up unless the event was successfully created.
  if (init_status == 0) {
    oboe_event_destroy(&event);
  }
}

//
// JavaScript constructor
//
// new Event(xtrace, addEdge = true)
//
// @param {Metadata|Event|string} xtrace - X-Trace ID to use for creating event
// @param boolean [addEdge]
//
//
Event::Event(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Event>(info) {
    Napi::Env env = info.Env();

    oboe_metadata_t omd;

    // indicate that the construction failed so the destructor won't ask oboe to free it.
    init_status = -1;

    if (info.Length() < 1) {
      Napi::TypeError::New(env, "invalid signature").ThrowAsJavaScriptException();
      return;
    }

    //Napi::Object o = info[0].As<Napi::Object>();
    Napi::Object o = info[0].ToObject();

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
        Napi::TypeError::New(env, "Event::New - invalid X-Trace ID string").ThrowAsJavaScriptException();
        return;
      }
    } else {
      Napi::TypeError::New(env, "no metadata found").ThrowAsJavaScriptException();
      return;
    }

    // here there is metadata in omd and that's all the information needed in order
    // to create an event. add an edge if the caller requests.
    bool add_edge = false;
    if (info.Length() >= 2) {
      add_edge = info[1].ToBoolean().Value();
    }

    // supply the metadata for the event. oboe_event_init() will create a new
    // random op ID for the event. (The op ID can be specified using the 3rd
    // argument but there is no benefit to doing so.)
    init_status = oboe_event_init(&this->event, &omd, NULL);
    if (init_status != 0) {
      Napi::Error::New(env, "oboe.event_init: " + std::to_string(init_status)).ThrowAsJavaScriptException();
      return;
    }

    if (add_edge) {
      int edge_status = oboe_event_add_edge(&this->event, &omd);
      if (edge_status != 0) {
        Napi::Error::New(env, "oboe.add_edge: " + std::to_string(edge_status)).ThrowAsJavaScriptException();
        return;
      }
    }
}

//
// C++ callable function to create a JavaScript Event object.
//
Napi::Object Event::NewInstance(Napi::Env env) {
    Napi::EscapableHandleScope scope(env);

    Napi::Object o = constructor.New({});

    return scope.Escape(napi_value(o)).ToObject();
}

//
// C++ callable function to create a JavaScript Event object.
//
// This signature includes an optional boolean for whether an edge
// should be set or not. It defaults to true.
//
Napi::Object Event::NewInstance(Napi::Env env, oboe_metadata_t* omd, bool edge) {
  Napi::EscapableHandleScope scope(env);

  Napi::Value v0 = Napi::External<oboe_metadata_t>::New(env, omd);
  Napi::Value v1 = Napi::Boolean::New(env, edge);

  Napi::Object o = constructor.New({v0, v1});

  return scope.Escape(napi_value(o)).ToObject();
}

//
// JavaScript callable method to format the event as a string.
//
// The string representation of an event is the string representation of
// the metadata part of the event.
//
Napi::Value Event::toString(const Napi::CallbackInfo& info) {
    oboe_metadata_t* md = &this->event.metadata;
    char buf[Metadata::fmtBufferSize];

    int rc;

    if (info.Length() == 1 && info[0].IsNumber()) {
      int fmt = info[0].As<Napi::Number>().Int64Value();
      // default 1 to a human readable form for historical reasons.
      if (fmt == 1) {
        fmt = Metadata::fmtHuman;
      }
      rc = Metadata::format(md, sizeof(buf), buf, fmt) ? 0 : -1;
    } else {
      rc = oboe_metadata_tostr(md, buf, sizeof(buf) - 1);
    }

    return Napi::String::New(info.Env(), rc == 0 ? buf : "");
}

//
// JavaScript callable method to set the event's sample flag to the boolean
// sense of the argument.
//
// returns the previous value of the flag.
//
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

//
// JavaScript callable method to get the sample flag from
// the event metadata.
//
// returns boolean
//
Napi::Value Event::getSampleFlag(const Napi::CallbackInfo& info) {
    return Napi::Boolean::New(info.Env(), this->event.metadata.flags & XTR_FLAGS_SAMPLED);
}

//
// JavaScript callable method to get a new metadata object with
// the same metadata as this event.
//
// Event.getMetadata()
//
// Return a Metadata object with the metadata from this event.
//
Napi::Value Event::getMetadata(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    oboe_metadata_t* omd = &this->event.metadata;

    Napi::Value v = Napi::External<oboe_metadata_t>::New(env, omd);
    return Metadata::NewInstance(env, v);
}

//
// JavaScript callable method to add an edge to the event.
//
// event.addEdge(edge)
//
// @param {Event | Metadata | string} X-Trace ID to edge back to
//
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

//
// JavaScript method to add info to the event.
//
// event.addInfo(key, value)
//
// @param {string} key
// @param {string | number | boolean} value
//
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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//
// return true if the object meets our expectations of what an Event should be
// otherwise return false.
//
bool validEvent (Napi::Object event) {
  if (!event.Has("Layer") || !event.Has("Label")) {
    return false;
  }
  if (!event.Has("_edges") || !event.Get("_edges").IsArray()) {
    return false;
  }

  Napi::Value kv = event.Get("kv");
  if (!kv.IsObject() || kv.IsArray()) {
    return false;
  }
  if (!kv.As<Napi::Object>().Has("Timestamp_u")) {
    return false;
  }

  Napi::Value mb = event.Get("mb");
  if (!mb.IsObject() || mb.IsArray()) {
    return false;
  }

  Napi::Value buf = mb.As<Napi::Object>().Get("buf");
  if (!buf.IsBuffer()) {
    return false;
  }
  Napi::Buffer<uint8_t> b = buf.As<Napi::Buffer<uint8_t>>();
  if (b.Length() != 30) {
    return false;
  }

  return true;
}

//
// initialize oboe_metadata_t structure from a Metabuf buffer
//
void initializeOboeMd(oboe_metadata_t &md, Napi::Buffer<uint8_t> b) {
  md.version = XTR_CURRENT_VERSION;

  // copy IDs
  const uint kTaskIdBase = 1;
  const uint kOpIdBase = kTaskIdBase + OBOE_MAX_TASK_ID_LEN;
  for (uint i = 0; i < OBOE_MAX_TASK_ID_LEN; i++) {
    md.ids.task_id[i] = b[kTaskIdBase + i];
  }
  for (uint i = 0; i < OBOE_MAX_OP_ID_LEN; i++) {
    md.ids.op_id[i] = b[kOpIdBase + i];
  }

  // initialize lengths
  md.task_len = OBOE_MAX_TASK_ID_LEN;
  md.op_len = OBOE_MAX_OP_ID_LEN;

  // and finally flags.
  md.flags = b[kOpIdBase + OBOE_MAX_OP_ID_LEN];
}

//
// add KV pair
//
// returns -1 if error.
//
int addKvPair(oboe_event_t* event, std::string key, Napi::Value value) {
  int status = -1;

  if (value.IsBoolean()) {
    status = oboe_event_add_info_bool(event, key.c_str(), value.As<Napi::Boolean>().Value());
  } else if (value.IsNumber()) {
    const double v = value.As<Napi::Number>();

    // it it's within integer range add an integer
    if (v > -(pow(2, 53) - 1) && v < pow(2, 53) -1) {
      status = oboe_event_add_info_int64(event, key.c_str(), v);
    } else {
      status = oboe_event_add_info_double(event, key.c_str(), v);
    }
  } else if (value.IsString()) {
    std::string s = value.As<Napi::String>();

    // make a stab at detecting binary data
    if (memchr(s.c_str(), '\0', s.length())) {
      status = oboe_event_add_info_binary(event, key.c_str(), s.c_str(), s.length());
    } else {
      status = oboe_event_add_info(event, key.c_str(), s.c_str());
    }
  }

  return status;
}

//
// Event send function. The edges and KVs have been validated
// by the JavaScript Event class so minimal checking is required.
//
Napi::Value Event::send(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Invalid signature").ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::Object event = info[0].As<Napi::Object>();
  bool useStatusChannel = info[1].ToBoolean().Value();

  if (!validEvent(event)) {
    Napi::TypeError::New(env, "Not a valid Event").ThrowAsJavaScriptException();
    return env.Null();
  }

  // return a results object.
  Napi::Object result = Napi::Object::New(env);

  Napi::Object kvs = event.Get("kv").ToObject();

  // this is really an array and validEvent() verified that but
  // an array behaves like an object with int32 indexes/properties.
  Napi::Object edges = event.Get("_edges").ToObject();

  Napi::Object mb = event.Get("mb").As<Napi::Object>().Get("buf").As<Napi::Object>();
  Napi::Buffer<uint8_t> buf = mb.As<Napi::Buffer<uint8_t>>();

  // return any errors here.
  Napi::Array errors = Napi::Array::New(env);

  oboe_metadata_t oboe_md;
  oboe_event_t oboe_event;

  initializeOboeMd(oboe_md, buf);

  oboe_event_init(&oboe_event, &oboe_md, const_cast<const uint8_t*>(oboe_md.ids.op_id));

  // add the KV pairs
  Napi::Array kvKeys = kvs.GetPropertyNames();
  for (uint i = 0; i < kvKeys.Length(); i++) {
    Napi::EscapableHandleScope scope(env);
    Napi::Value v = kvKeys[i];
    std::string key = v.ToString();
    int status = addKvPair(&oboe_event, key, kvs.Get(key));
    if (status != 0) {
      Napi::Object err = Napi::Object::New(env);
      err.Set("code", "kv send failed");
      err.Set("key", key);
      errors[errors.Length()] = err;
      scope.Escape(err);
    }
  }

  Napi::Array edgeIndexes = edges.GetPropertyNames();

  /*
  for (uint i = 0; i < OBOE_MAX_TASK_ID_LEN; i++) {
    std::cout << std::hex << std::setfill('0') << std::setw(2) << (uint)oboe_md.ids.task_id[i];
  }
  std::cout << ":";
  for (uint i = 0; i < OBOE_MAX_OP_ID_LEN; i++) {
    std::cout << std::hex << std::setfill('0') << std::setw(2) << (uint)oboe_md.ids.op_id[i];
  }
  std::cout << "\n";

  Napi::Array props = event.GetPropertyNames();
  Napi::Array keys = Napi::Array::New(env);


  for (uint i = 0; i < props.Length(); i++) {
    Napi::Value key = props[i];
    //keys[i] = key.ToString();
  }

  result.Set("keys", keys);
  // */

  result.Set("status", errors.Length() == 0);
  result.Set("errors", errors);
  result.Set("statusChannel", useStatusChannel);
  result.Set("edgeIndexes", edgeIndexes);

  /*
  oboe_event_init(&event, &metadata, &opid);         // use event metadata and opid

  loop:
  oboe_event_add_info<_type> (&event, key, value);   // for all agent KV + hostname

  loop: oboe_event_add_edge(&event, edge);           // if any

  oboe_bson_buffer_finish(&event);

  oboe_raw_send(&event);
  */

  return result;
}

//
// C++ callable method to determine if object is a JavaScript Event
// instance.
//
bool Event::isEvent(Napi::Object o) {
  return o.IsObject() && o.InstanceOf(constructor.Value());
}

//
// initialize the module and expose the Event class.
//
Napi::Object Event::Init(Napi::Env env, Napi::Object exports) {
  Napi::HandleScope scope(env);

  Napi::Function ctor = DefineClass(
      env, "Event", {
        InstanceMethod("addInfo", &Event::addInfo),
        InstanceMethod("addEdge", &Event::addEdge),
        InstanceMethod("getMetadata", &Event::getMetadata),
        InstanceMethod("toString", &Event::toString),
        InstanceMethod("getSampleFlag", &Event::getSampleFlag),
        InstanceMethod("setSampleFlagTo", &Event::setSampleFlagTo),
        StaticMethod("send", &Event::send),
        StaticValue("xtraceIdVersion", Napi::Number::New(env, XTR_CURRENT_VERSION)),
        StaticValue("taskIdLength", Napi::Number::New(env, OBOE_MAX_TASK_ID_LEN)),
        StaticValue("opIdLength", Napi::Number::New(env, OBOE_MAX_OP_ID_LEN)),
      });

  constructor = Napi::Persistent(ctor);
  constructor.SuppressDestruct();

  exports.Set("Event", ctor);

  return exports;
}
