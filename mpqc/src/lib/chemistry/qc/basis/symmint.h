
#ifndef _chemistry_qc_integral_symmint_h
#define _chemistry_qc_integral_symmint_h

#ifdef __GNUC__
#pragma interface
#endif

#include <util/state/state.h>
#include <chemistry/qc/basis/obint.h>
#include <chemistry/qc/basis/tbint.h>
#include <chemistry/qc/basis/petite.h>

////////////////////////////////////////////////////////////////////////////

class SymmOneBodyIntIter : public OneBodyIntIter {
  protected:
    RefPetiteList pl;
    
  public:
    SymmOneBodyIntIter(const RefOneBodyInt&, const RefPetiteList&);
    ~SymmOneBodyIntIter();

    void start(int ist=0, int jst=0, int ien=0, int jen=0);
    void next();

    double scale() const;
};

class SymmTwoBodyIntIter : public TwoBodyIntIter {
  protected:
    RefPetiteList pl;

  public:
    SymmTwoBodyIntIter(const RefTwoBodyInt&, const RefPetiteList&);
    ~SymmTwoBodyIntIter();

    void start();
    void next();

    double scale() const;
};

#endif

// Local Variables:
// mode: c++
// eval: (c-set-style "ETS")
