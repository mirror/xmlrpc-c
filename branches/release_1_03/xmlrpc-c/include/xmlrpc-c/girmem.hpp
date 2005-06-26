#ifndef GIRMEM_HPP_INCLUDED
#define GIRMEM_HPP_INCLUDED


namespace girmem {

class autoObjectPtr;

class autoObject {
    friend class autoObjectPtr;

public:
    void incref();
    void decref(bool * const unreferencedP);
    
protected:
    autoObject();
    virtual ~autoObject();

private:
    pthread_mutex_t refcountLock;
    unsigned int refcount;
};

class autoObjectPtr {
public:
    autoObjectPtr();
    autoObjectPtr(girmem::autoObject * objectP);
    autoObjectPtr(girmem::autoObjectPtr const& autoObjectPtr);
    
    ~autoObjectPtr();
    
    void
    instantiate(girmem::autoObject * const objectP);
    
    autoObjectPtr
    operator=(girmem::autoObjectPtr const& objectPtr);
    
    girmem::autoObject *
    operator->() const;
    
protected:
    girmem::autoObject * objectP;
};

} // namespace

#endif
