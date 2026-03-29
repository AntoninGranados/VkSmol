#pragma once

template<typename HandleT, HandleT NullHandle>
class BorrowedHandle {
public:
    HandleT& get() { return handle; }
    const HandleT& get() const { return handle; }

    bool isValid() const { return handle != NullHandle; }
    
protected:
      void resetHandle() { handle = NullHandle; }
      HandleT& rawHandle() { return handle; }
      const HandleT& rawHandle() const { return handle; }

private:
    HandleT handle = NullHandle;
};
