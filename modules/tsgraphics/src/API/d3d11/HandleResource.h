/*
	Render API
	
	D3D11Texture class
*/

#pragma once

#include "base.h"
#include "handle.h"

/////////////////////////////////////////////////////////////////////////////////////////////////

namespace ts
{
	class D3D11Resource : public Handle<D3D11Resource, ResourceHandle>
	{
	private:

		ComPtr<ID3D11Resource> m_rsc;
		bool m_isImage;

		struct SRV
		{
			uint32 arrayIndex = 0;
			uint32 arrayCount = 0;
			ImageType type;

			bool operator==(const SRV& other)
			{
				return other.arrayCount == arrayCount && other.arrayIndex == arrayIndex && other.type == type;
			}
		};

		struct SRVHash
		{
			size_t operator()(const SRV& srv)
			{
				return (((size_t)srv.type * 8) + srv.arrayCount) * 310348631 + srv.arrayIndex;
			}
		};

		template<typename Key, typename Interface, typename Hash = std::hash<Key>>
		using Cache = std::unordered_map<Key, ComPtr<Interface>, Hash>;

		/*
			View caches
		*/
		Cache<SRV, ID3D11ShaderResourceView, SRVHash> m_srvCache;
		Cache<uint32, ID3D11RenderTargetView> m_rtvCache;
		Cache<uint32, ID3D11DepthStencilView> m_dsvCache;

	public:

		D3D11Resource(ID3D11Resource* rsc, bool isImage) : m_rsc(rsc), m_isImage(isImage) {}
		~D3D11Resource() { reset(); }
		
		ID3D11Resource* getResource() const { m_rsc.Get(); }
		ID3D11Buffer* getBuffer() const { return (isImage()) ? nullptr : static_cast<ID3D11Buffer*>(m_rsc.Get()); }
		bool isImage() const { return m_isImage; }
		
		ID3D11ShaderResourceView* getSRV(uint32 arrayIndex, uint32 arrayCount, ImageType type);
		ID3D11RenderTargetView* getRTV(uint32 arrayIndex);
		ID3D11DepthStencilView* getDSV(uint32 arrayIndex);

		void reset()
		{
			m_rsc.Reset();
			m_srvCache.clear();
			m_rtvCache.clear();
			m_dsvCache.clear();
		}
	};
}

/////////////////////////////////////////////////////////////////////////////////////////////////
