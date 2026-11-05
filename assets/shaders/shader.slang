[shader("vertex")]
float4 vertMain() {
	return float4(0, 1, 0, 1);
}

[shader("fragment")]
float4 fragMain() : SV_Target {
	return float4(1, 0, 0, 1);
}