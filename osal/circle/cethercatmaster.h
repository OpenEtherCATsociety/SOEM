#ifndef CETHERCATMASTER_H
#define CETHERCATMASTER_H

class CEtherCATMaster
{
public:
    CEtherCATMaster(bool use_scheduler);

    bool IsSchedulerFriendly() const;

    static bool IsActive();
    static CEtherCATMaster* Get();

private:

    bool _use_scheduler;
};

#endif // CETHERCATMASTER_H
