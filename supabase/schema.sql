-- XIAO ESP32-S3 remote control state (one row per device).
create table if not exists public.device_state (
  id          text primary key,
  led         boolean     not null default false,
  touched     boolean     not null default false,
  touch_value integer     not null default 0,
  updated_at  timestamptz not null default now()
);

insert into public.device_state (id) values ('xiao') on conflict (id) do nothing;

-- Demo policy: anyone with the anon key can read/update this row.
alter table public.device_state enable row level security;
create policy "anon read"   on public.device_state for select to anon using (true);
create policy "anon update" on public.device_state for update to anon using (true) with check (true);

-- Push changes to the dashboard in real time.
alter publication supabase_realtime add table public.device_state;

-- Make sure the Data API roles can reach the table.
grant select, update on public.device_state to anon, authenticated;
notify pgrst, 'reload schema';

-- LAN camera: board reports its local IP and whether the camera started.
alter table public.device_state add column if not exists ip text;
alter table public.device_state add column if not exists camera boolean not null default false;
notify pgrst, 'reload schema';
