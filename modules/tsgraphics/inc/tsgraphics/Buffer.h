/*
	Buffer objects
*/

#pragma once

#include "Driver.h"

namespace ts
{
	/*
		Simple buffer class
	*/
	class Buffer : private RPtr<ResourceHandle>
	{
	public:

		using Base = RPtr<ResourceHandle>;

		using Base::handle;
		using Base::device;
		using Base::null;
		using Base::operator bool;

		Buffer() {}
		Buffer(Buffer&& rhs) { *this = std::move(rhs); }
		Buffer& operator=(Buffer&& rhs) { (Base&)*this = (Base&&)rhs; return *this; }

		Buffer(const Buffer&) = delete;
		void operator=(const Buffer&) = delete;

		/*
			Construct an empty staging buffer with a reserved handle
		*/
		Buffer(RenderDevice* dev)
		{
			((Base&)*this) = dev->createEmptyResource();
		}
		
		/*
			Construct a buffer
		*/
		Buffer(RenderDevice* dev, const ResourceData& data, const BufferResourceInfo& info)
		{
			((Base&)*this) = dev->createResourceBuffer(data, info);
		}

		/*
			Create a buffer a given pointer to memory
		*/
		static Buffer create(RenderDevice* device, const void* ptr, uint32 size, BufferType type)
		{
			ResourceData d;
			d.memory = ptr;
			BufferResourceInfo i;
			i.size = size;
			i.type = type;

			return Buffer(device, d, i);
		}

		/*
			Create a buffer from a typed object
		*/
		template<
			typename StructType
			//typename = std::enable_if<std::is_pod<StructType>::value>::type
		>
		static Buffer create(RenderDevice* device, const StructType& data, BufferType type)
		{
			return Buffer::create(device, (const void*)&data, sizeof(StructType), type);
		}
	};

	/*
		Typed constant buffer
	*/
	template<typename StructType>
	class ConstantBuffer : private Buffer
	{
	public:

		ConstantBuffer() {}
		ConstantBuffer(const ConstantBuffer&) = delete;

		ConstantBuffer(ConstantBuffer&& rhs)
		{
			((Buffer&)*this) = (Buffer&&)rhs;
			m_constants = rhs.m_constants;
		}

		ConstantBuffer& operator=(ConstantBuffer&& rhs)
		{
			((Buffer&)*this) = (Buffer&&)rhs;
			m_constants = rhs.m_constants;
			return *this;
		}

		ConstantBuffer(RenderDevice* device, const StructType& initial)
		{
			m_constants = initial;
			((Buffer&)*this) = Buffer::create(device, initial, BufferType::CONSTANTS);
		}

		StructType& get() { return m_constants; }
		const StructType& get() const { return m_constants; }
		void set(const StructType& data) { m_constants = data; }

		void commit(RenderContext* context)
		{
			context->resourceUpdate(Buffer::handle(), &m_constants);
		}

		using Buffer::handle;
		using Buffer::device;
		using Buffer::null;
		using Buffer::operator bool;

	private:

		StructType m_constants;
	};
}
