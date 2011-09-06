
#ifndef _mpqcinit_h
#define _mpqcinit_h

#include <scconfig.h>
#include <util/options/GetLongOpt.h>
#include <util/keyval/keyval.h>
#include <util/group/message.h>
#include <util/group/thread.h>
#include <util/group/memory.h>
#include <string>

namespace sc {

  /// This helper class simplifies initialization of MPQC.
  class MPQCInit {
    GetLongOpt& opt_;
    char **argv_;
    int &argc_;
  public:
    /// Create the initializer. Needed options will be enrolled
    /// in the opt object. The parse member of opt must be called
    /// after this constructor completes, but before any of the
    /// other members of MPQCInit are called.
    MPQCInit(GetLongOpt&opt, int &argc, char **argv);
    ~MPQCInit();
    /// Initialize the floating point control word.
    void init_fp();
    /// Initialize the resource limits.
    void init_limits();
    /// Return the initial MessageGrp object.
    sc::Ref<sc::MessageGrp> init_messagegrp();
    /// Return the initial KeyVal object.
    sc::Ref<sc::KeyVal> init_keyval(const sc::Ref<sc::MessageGrp> &grp,
                                    const std::string &filename);

    /// @defgroup init These members initialize MPQC defaults (such as ThreadGrp, Integral, etc.)
    /// All try environment first, then @a keyval (which may be null), to initialize the corresponding default object
    /// @{
    /// Return the initial ThreadGrp.
    sc::Ref<sc::ThreadGrp>
      init_threadgrp(Ref<sc::KeyVal> keyval = Ref<KeyVal>());
    /// Return the initial MemoryGrp.
    sc::Ref<sc::MemoryGrp>
      init_memorygrp(Ref<sc::KeyVal> keyval = Ref<KeyVal>());
    /// Initialize the default integral factory.
    void init_integrals(Ref<KeyVal> keyval = Ref<KeyVal>());
    /// Initialize the default ConsumableResources object.
    void init_resources(Ref<KeyVal> keyval = Ref<KeyVal>());
    /// Initialize the default region timer.
    void init_timer(const Ref<MessageGrp> &grp, Ref<KeyVal> keyval = Ref<KeyVal>());
    /// @}

    /// Initialize formatted I/O.
    void init_io(const sc::Ref<sc::MessageGrp> &grp);
    /// Initialize the name used to construct data file names.
    void init_basename(const std::string &input_filename,
                       const std::string &output_filename = "");
    /// Calls all of the initialize routines in the proper sequence.
    /// The parse member for the GetLongOpt object given to the
    /// constructor must have been called before this is called.
    sc::Ref<sc::KeyVal> init(const std::string &input_filename,
                             const std::string &output_filename = "");
    /// Clean up at the end of a run. This is called automatically by
    /// the destructor.
    void finalize();
  };

}

#endif
