#pragma once
#include <boost/asio.hpp>

#include <queue>

namespace appbase {
// adapted from: https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/example/cpp11/invocation/prioritised_handlers.cpp

struct priority {
   static constexpr int high = 100;
   static constexpr int medium = 50;
   static constexpr int low = 10;
};

class execution_priority_queue : public boost::asio::execution_context
{
public:

   template <typename Function>
   void add(int priority, Function function)
   {
      std::unique_ptr<queued_handler_base> handler(new queued_handler<Function>(priority, std::move(function)));

      handlers_.push(std::move(handler));
   }

   void execute_all()
   {
      while (!handlers_.empty())
      {
         handlers_.top()->execute();
         handlers_.pop();
      }
   }

   bool execute_highest()
   {
      // execute at least the highest priority, and all of the available >= priority::high priority
      while( !handlers_.empty() )
      {
         const auto& top = handlers_.top();
         const int priority = top->priority();
         top->execute();
         handlers_.pop();
         if( priority < priority::high ) {
            // execute only one after executing all of high
            break;
         }
      }

      return !handlers_.empty();
   }

   void execute_high() {
      // execute priority::high or higher
      while( !handlers_.empty() )
      {
         const auto& top = handlers_.top();
         const int priority = top->priority();
         if( priority < priority::high ) {
            break;
         }
         top->execute();
         handlers_.pop();
      }
   }

   class executor
   {
   public:
      executor(execution_priority_queue& q, int p)
            : context_(q), priority_(p)
      {
      }

      execution_priority_queue& context() const noexcept
      {
         return context_;
      }

      template <typename Function, typename Allocator>
      void dispatch(Function f, const Allocator&) const
      {
         context_.add(priority_, std::move(f));
      }

      template <typename Function, typename Allocator>
      void post(Function f, const Allocator&) const
      {
         context_.add(priority_, std::move(f));
      }

      template <typename Function, typename Allocator>
      void defer(Function f, const Allocator&) const
      {
         context_.add(priority_, std::move(f));
      }

      void on_work_started() const noexcept {}
      void on_work_finished() const noexcept {}

      bool operator==(const executor& other) const noexcept
      {
         return &context_ == &other.context_ && priority_ == other.priority_;
      }

      bool operator!=(const executor& other) const noexcept
      {
         return !operator==(other);
      }

   private:
      execution_priority_queue& context_;
      int priority_;
   };

   template <typename Function>
   boost::asio::executor_binder<Function, executor>
   wrap(int priority, Function&& func)
   {
      return boost::asio::bind_executor( executor(*this, priority), std::forward<Function>(func) );
   }

private:
   class queued_handler_base
   {
   public:
      queued_handler_base(int p)
            : priority_(p)
      {
      }

      virtual ~queued_handler_base()
      {
      }

      virtual void execute() = 0;

      int priority() const { return priority_; }

      friend bool operator<(const std::unique_ptr<queued_handler_base>& a,
                            const std::unique_ptr<queued_handler_base>& b) noexcept
      {
         return a->priority_ < b->priority_;
      }

   private:
      int priority_;
   };

   template <typename Function>
   class queued_handler : public queued_handler_base
   {
   public:
      queued_handler(int p, Function f)
            : queued_handler_base(p), function_(std::move(f))
      {
      }

      void execute() override
      {
         function_();
      }

   private:
      Function function_;
   };

   std::priority_queue<std::unique_ptr<queued_handler_base>, std::deque<std::unique_ptr<queued_handler_base>>> handlers_;
};

} // appbase
