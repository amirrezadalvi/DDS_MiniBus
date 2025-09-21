# Design Rationale (چرایی تصمیم‌ها)

## Language & Framework (زبان و چارچوب)
- **Choice:** C++17 + Qt Network  
- **Why:** High performance, mature networking, cross-platform. (کارایی بالا، کتابخانه شبکه بالغ، چندسکویی)

## Transport (لایه انتقال)
- **Choice:** UDP (Best-Effort) + app-level Reliable via ACK; TCP available  
- **Why:** UDP ساده و کم‌هزینه برای broadcast/discovery؛ Reliable با ACK قابل حمل روی UDP؛ TCP برای سناریوهای نیازمند اتصال.  
- **Trade-off:** UDP نیازمند منطق ACK/Retry است. (UDP به منطق ACK/Retry نیاز دارد.)

## Discovery (کشف نودها)
- **Choice:** Broadcast/Multicast + Loopback برای ویندوز  
- **Why:** ساده، بدون وابستگی به سرویس خارجی؛ Loopback برای محیط‌های محدود/فایروال ویندوز.  
- **Configurable:** از طریق `config/*.json` و متغیر محیطی `DDS_TEST_LOOPBACK`.

## Message Format (فرمت پیام)
- **Choice:** JSON + CBOR  
- **Why:** خوانایی/دیباگ آسان (JSON) و کارایی بهتر (CBOR). Negotiation فرمت در زمان اتصال.

## QoS
- **Levels:** Best-Effort, Reliable(ACK/Retry/Timeout)  
- **Defaults:** مقادیر پیش‌فرض در config و امکان تنظیم.  

## Testing Strategy (استراتژی تست)
- **Unit/Perf Tests:** serializer, negotiation, ack, latency/throughput.  
- **Integration:** pub2sub, discovery, qos_failure.  
- **Windows Note:** تست «integration_scenarios» تک‌پردازه‌ای روی MinGW ناپایدار است → **Disabled** با گزینه override برای CI. این تصمیم مستند و مطابق محیط ویندوز است.

## Risks & Mitigations (ریسک‌ها و پوشش‌ها)
- Multicast محدود در ویندوز → Loopback و Skip کنترل‌شده.  
- وابستگی به Qt DLL در تست‌ها → اجرای CTest از `qt_deploy` و وین‌دیپلوی.

