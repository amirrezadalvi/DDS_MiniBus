# معماری DDS Mini‑Bus


این سند، معماری سطح‌بالا و رفتار زمان‌اجرا در پیاده‌سازی DDS Mini‑Bus را توضیح می‌دهد. برای اختصار، دیاگرام‌ها با PlantUML در `docs/diagrams/` آورده شده‌اند.


## اهداف
- پیام‌رسانی Pub/Sub با مسیریابی مبتنی بر «موضوع» (Topic)
- اکتشاف (Discovery) سبک با حالت‌های **Broadcast/Multicast** و حالت **Loopback** برای دموهای لوکال
- دو ترنسپورت: **UDP** و **TCP**
- دو فرمت سریال‌سازی: **JSON** و **CBOR** با مذاکرهٔ (Negotiation) زمان اجرا
- QoS: **Best‑Effort** و **Reliable** (ACK + تلاش مجدد محدود با **Backoff نمایی**)
- پیکربندی سادهٔ JSON با مقادیر پیش‌فرض منطقی و **امکان به‌روزرسانی بخشی از پارامترها در زمان اجرا**


## نمای کلی معماری
به دیاگرام کامپوننت مراجعه کنید: `docs/diagrams/component.puml`.


**ماژول‌های کلیدی:**


- **DDSCore**
هماهنگ‌کنندهٔ چرخهٔ عمر نود. رجیستری‌های Publisher/Subscriber را نگه می‌دارد، مالک `DiscoveryManager`، یک نمونهٔ `TransportBase` (پیش‌فرض UDP) و یک `Serializer` است. بر هر Peer مذاکرهٔ فرمت انجام می‌دهد و پیام‌ها را به همتایان واجد شرایط مسیر می‌دهد.


- **Publisher / Subscriber**
*Publisher* دارای `(topic, qos, formatPreference)` است و Payload را به Core می‌سپارد. *Subscriber* برای یک Topic مشترک می‌شود و آبجکت‌های دیکدشده را تحویل می‌گیرد؛ در صورت نیاز Cache آخرین پیام را نگه می‌دارد.


- **DiscoveryManager**
حضور و قابلیت‌های همتا (Topicها، پورت داده، فرمت‌های قابل پشتیبانی، نسخهٔ پروتکل) را اعلان/یادگیری می‌کند. حالت‌ها:
- **Multicast** (مثل 239.255.0.1)
- **Broadcast** (255.255.255.255)
- **Loopback** (با `DDS_TEST_LOOPBACK=1` برای تست‌های لوکال)
رویدادهایی مثل `peerUpdated` را امیت می‌کند که Core از آن برای به‌روزرسانی مسیریابی استفاده می‌کند.


- **TransportBase / UdpTransport / TcpTransport**
سطح مشترک ارسال/دریافت پاکت‌ها (Envelope). `UdpTransport` صفحهٔ دادهٔ پیش‌فرض است (Unicast به پورت دادهٔ Peer)؛ `TcpTransport` در تست‌های مرتبط استفاده می‌شود. QoS نوع Reliable از `AckManager` برای رهگیری پیام‌های در جریان، Timeoutها و Retry بهره می‌گیرد.


- **AckManager**
نگاشت `message_id` ← وضعیت تحویل برای QoS قابل‌اعتماد. تلاش مجدد محدود با Backoff نمایی را پیاده‌سازی می‌کند و پس از اتمام تلاش‌ها، هشدار/Dead‑Letter ثبت می‌کند.


- **Serializer**
از **JSON** و **CBOR** پشتیبانی می‌کند. هنگام برقراری لینک، Core روی فرمت مشترک مذاکره می‌کند (ترجیح JSON).


- **ConfigManager**
`config.json` را بارگیری می‌کند که شامل حالت/پورت‌های Discovery، پورت داده، تنظیمات QoS و Logging است. برخی پارامترها بدون ری‌استارت قابل Reload هستند.


- **Logger (Qt Categories)**
دسته‌هایی مانند `dds.net` و `dds.disc`. برای دمو/تست می‌توانید با `QT_LOGGING_RULES="dds.disc=true;dds.net=true"` لاگ‌های جزئی را فعال کنید.


## مدل داده (Envelope)
فیلدهای حداقلی روی سیم:
- `topic` (رشته)
- `message_id` (عدد بدون علامت، صعودی به ازای هر نود)
- `qos` (مقادیر "best_effort" یا "reliable")
- `format` ("json" یا "cbor")
- `payload` (بایت‌ها) — خروجی Serializer انتخاب‌شده


**ACK** شامل `message_id` اصلی است و هنگام `qos == reliable` از گیرنده ارسال می‌شود.


## پیام‌های Discovery
اعلان‌های دوره‌ای شامل:
- `node_id` (رشته)
- `version` (رشته)
- `topics` (فهرست)
- `formats` (فهرست، مانند ["json","cbor"])
- `dataPort` (عدد)


## QoS (Reliable)
- تخصیص `message_id` و ارسال به همهٔ همتایان مسیرشده
- آغاز رهگیری در `AckManager` با Timeout اولیهٔ `T` و ضریب Backoff `k` (مثلاً ۲×)
- در Timeout، تلاش مجدد تا سقف `N`
- در دریافت ACK، تحویل موفق ثبت و تلاش‌ها متوقف می‌شود
- پس از `N` شکست، Dead‑Letter/هشدار ثبت می‌گردد

دیاگرام توالی `docs/diagrams/sequence_publish_reliable.puml` این جریان را نشان می‌دهد.


## Threading و حلقهٔ رخداد
حلقهٔ رخداد Qt، تایمرها (Retryها، Beaconهای Discovery) و I/O سوکت‌ها را پیش می‌برد. روی Windows/MinGW، تست Integration تک‌پروسه که چند Core را در یک پروسه می‌سازد ممکن است ناپایدار باشد؛ به همین دلیل این تست به‌صورت پیش‌فرض **غیرفعال** است و پوشش E2E با دموهای چندپروسه (PowerShell) انجام می‌شود.


## پیکربندی
`config/config.json` با بیلد آرته‌فکت‌ها به مسیر `build/qt_deploy/config/config.json` کپی می‌شود. فیلدهای کلیدی:
- `discovery.mode`: یکی از `multicast` | `broadcast` | `loopback`
- `discovery.group`: گروه IPv4 یا 255.255.255.255 (برای Broadcast)
- `discovery.port`: پورت کنترل (مثلاً 39001)
cd .\build\qt_deploy; .\demo_tx.exe --config config\config.json