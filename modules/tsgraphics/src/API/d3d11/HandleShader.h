/*
	Render API

	Shader program class
*/

#include "render.h"
#include "handle.h"

namespace ts
{
	struct D3D11Shader : public Handle<D3D11Shader, ShaderHandle>
	{
		//Shader stages
		ComPtr<ID3D11VertexShader> vertex;
		ComPtr<ID3D11GeometryShader> geometry;
		ComPtr<ID3D11DomainShader> domain;
		ComPtr<ID3D11HullShader> hull;
		ComPtr<ID3D11PixelShader> pixel;
		ComPtr<ID3D11ComputeShader> compute;

		//Bind shader program stages
		void bind(ID3D11DeviceContext* context);
	};
}
