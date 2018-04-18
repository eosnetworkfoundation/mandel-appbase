#pragma once

//clashes with BOOST PP and Some Applications
#pragma push_macro("N")
#undef N

#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <boost/exception/diagnostic_information.hpp>

namespace appbase {

   using erased_channel_ptr = std::unique_ptr<void, void(*)(void*)>;

   struct drop_exceptions {
      drop_exceptions() = default;
      using result_type = void;

      template<typename InputIterator>
      result_type operator()(InputIterator first, InputIterator last) {
         while (first != last) {
            try {
               *first;
            } catch (...) {
               // drop
            }
            ++first;
         }
      }
   };

   /**
    * A channel is a loosely bound asynchronous data pub/sub concept.
    *
    * This removes the need to tightly couple different plugins in the application for the use-case of
    * sending data around
    *
    * Data passed to a channel is *copied*, consider using a shared_ptr if the use-case allows it
    *
    * @tparam Data - the type of data to publish
    */
   template<typename Data, typename DispatchPolicy>
   class channel final : private boost::signals2::signal<void(const Data&), DispatchPolicy> {
      public:
         using ios_ptr_type = std::shared_ptr<boost::asio::io_service>;
         using handle_type = boost::signals2::connection;

         /**
          * Publish data to a channel
          * @param data
          */
         void publish(const Data& data) {
            if (has_subscribers()) {
               // this will copy data into the lambda
               ios_ptr->post([this, data]() {
                  (*this)(data);
               });
            }
         }

         /**
          * subscribe to data on a channel
          * @tparam Callback the type of the callback (functor|lambda)
          * @param cb the callback
          * @return handle to the subscription
          */
         template<typename Callback>
         handle_type subscribe(Callback cb) {
            return this->connect(cb);
         }

         /**
          * unsubscribe from data on a channel
          * @param handle
          */
         void unsubscribe(const handle_type& handle) {
            handle.disconnect();
         }

         /**
          * set the dispatcher according to the DispatchPolicy
          * this can be used to set a stateful dispatcher
          *
          * This method is only available when the DispatchPolicy is copy constructible due to implementation details
          *
          * @param policy - the DispatchPolicy to copy
          */
         auto set_dispatcher(const DispatchPolicy& policy ) -> std::enable_if_t<std::is_copy_constructible<DispatchPolicy>::value,void>
         {
            (*this).set_combiner(policy);
         }

         /**
          * Returns whether or not there are subscribers
          */
         bool has_subscribers() {
            return (*this).num_slots() > 0;
         }

      private:
         channel(const ios_ptr_type& ios_ptr)
         :ios_ptr(ios_ptr)
         {
         }

         virtual ~channel() {};

         /**
          * Proper deleter for type-erased channel
          * note: no type checking is performed at this level
          *
          * @param erased_channel_ptr
          */
         static void deleter(void* erased_channel_ptr) {
            channel *ptr = reinterpret_cast<channel*>(erased_channel_ptr);
            delete ptr;
         }

         /**
          * get the channel back from an erased pointer
          *
          * @param ptr - the type-erased channel pointer
          * @return - the type safe channel pointer
          */
         static channel* get_channel(erased_channel_ptr& ptr) {
            return reinterpret_cast<channel*>(ptr.get());
         }

         /**
          * Construct a unique_ptr for the type erased method poiner
          * @return
          */
         static erased_channel_ptr make_unique(const ios_ptr_type& ios_ptr)
         {
            return erased_channel_ptr(new channel(ios_ptr), &deleter);
         }

         ios_ptr_type ios_ptr;

         friend class appbase::application;
   };

   template< typename Tag, typename Data, typename DispatchPolicy = drop_exceptions >
   struct channel_decl {
      using channel_type = channel<Data, DispatchPolicy>;
      using tag_type = Tag;
   };

   template <typename...Ts>
   std::true_type is_channel_decl_impl(const channel_decl<Ts...>*);

   std::false_type is_channel_decl_impl(...);

   template <typename T>
   using is_channel_decl = decltype(is_channel_decl_impl(std::declval<T*>()));
}

#pragma pop_macro("N")
